#include "stream_outlet_impl.h"
#include "api_config.h"
#include "sample.h"
#include "send_buffer.h"
#include "stream_info_impl.h"
#include "tcp_server.h"
#include "udp_server.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <type_traits>

namespace lsl {

stream_outlet_impl::stream_outlet_impl(const stream_info_impl &info, int32_t chunk_size,
	int32_t requested_bufsize, lsl_transport_options_t flags)
	: sample_factory_(std::make_shared<factory>(info.channel_format(), info.channel_count(),
		  static_cast<uint32_t>(
			  info.nominal_srate()
				  ? info.nominal_srate() * api_config::get_instance()->outlet_buffer_reserve_ms() /
						1000
				  : api_config::get_instance()->outlet_buffer_reserve_samples()))),
	  chunk_size_(info.calc_transport_buf_samples(requested_bufsize, flags)),
	  info_(std::make_shared<stream_info_impl>(info)),
	  send_buffer_(std::make_shared<send_buffer>(chunk_size_)),
	  io_ctx_data_(std::make_shared<asio::io_context>(1)),
	  io_ctx_service_(std::make_shared<asio::io_context>(1)),
	  sync_mode_((flags & transp_sync_blocking) != 0) {
	ensure_lsl_initialized();

	// Validate sync mode constraints
	if (sync_mode_) {
		if (info.channel_format() == cft_string) {
			throw std::invalid_argument(
				"Synchronous (zero-copy) mode is not supported for string-format streams");
		}
		LOG_F(INFO, "Creating outlet in synchronous (zero-copy) mode for stream '%s'",
			info.name().c_str());
	}

	const api_config *cfg = api_config::get_instance();

	// instantiate IPv4 and/or IPv6 stacks (depending on settings)
	if (cfg->allow_ipv4()) try {
			instantiate_stack(udp::v4());
		} catch (std::exception &e) {
			LOG_F(WARNING, "Could not instantiate IPv4 stack: %s", e.what());
		}
	if (cfg->allow_ipv6()) try {
			instantiate_stack(udp::v6());
		} catch (std::exception &e) {
			LOG_F(WARNING, "Could not instantiate IPv6 stack: %s", e.what());
		}

	// create TCP data server
	tcp_server_ = std::make_shared<tcp_server>(info_, io_ctx_data_, send_buffer_, sample_factory_,
		chunk_size_, cfg->allow_ipv4(), cfg->allow_ipv6(), sync_mode_);

	// fail if both stacks failed to instantiate
	if (udp_servers_.empty())
		throw std::runtime_error("Neither the IPv4 nor the IPv6 stack could be instantiated.");

	// get the async request chains set up
	tcp_server_->begin_serving();
	for (auto &udp_server : udp_servers_) udp_server->begin_serving();
	for (auto &responder : responders_) responder->begin_serving();

	// and start the IO threads to handle them
	const std::string name{"IO_" + this->info().name().substr(0, 11)};
	for (const auto &io : {io_ctx_data_, io_ctx_service_})
		io_threads_.emplace_back(std::make_shared<std::thread>([io, name]() {
			loguru::set_thread_name(name.c_str());
			while (true) {
				try {
					io->run();
					return;
				} catch (std::exception &e) {
					LOG_F(ERROR, "Error during io_context processing: %s", e.what());
				}
			}
		}));
}

void stream_outlet_impl::instantiate_stack(udp udp_protocol) {
	// get api_config
	const api_config *cfg = api_config::get_instance();
	std::string listen_address = cfg->listen_address();
	int multicast_ttl = cfg->multicast_ttl();
	uint16_t multicast_port = cfg->multicast_port();
	LOG_F(2, "%s: Trying to listen at address '%s'", info().name().c_str(), listen_address.c_str());

	// create UDP time server
	udp_servers_.push_back(std::make_shared<udp_server>(info_, *io_ctx_service_, udp_protocol));
	// create UDP multicast responders
	for (const auto &address : cfg->multicast_addresses()) {
		try {
			// use only addresses for the protocol that we're supposed to use here
			if (udp_protocol == udp::v4() ? address.is_v4() : address.is_v6())
				responders_.push_back(std::make_shared<udp_server>(
					info_, *io_ctx_service_, address, multicast_port, multicast_ttl, listen_address));
		} catch (std::exception &e) {
			LOG_F(WARNING, "Couldn't create multicast responder for %s (%s)",
				address.to_string().c_str(), e.what());
		}
	}
}

stream_outlet_impl::~stream_outlet_impl() {
	try {
		// cancel all request chains
		tcp_server_->end_serving();
		for (auto &udp_server : udp_servers_) udp_server->end_serving();
		for (auto &responder : responders_) responder->end_serving();

		// In theory, an io context should end quickly, but in practice it
		// might take a while. So we
		// 1. ask them to stop after they've finished their current task
		// 2. wait a bit
		// 3. stop the io contexts from our thread. Not ideal, but better than
		// 4. waiting a bit and
		// 5. detaching thread, i.e. letting it hang and continue tearing down
		//    the outlet
		asio::post(*io_ctx_data_, [io = io_ctx_data_]() { io->stop(); });
		asio::post(*io_ctx_service_, [io = io_ctx_service_]() { io->stop(); });
		const char *name = this->info().name().c_str();
		for (int try_nr = 0; try_nr <= 100; ++try_nr) {
			switch (try_nr) {
			case 0: DLOG_F(INFO, "Trying to join IO threads for %s", name); break;
			case 20: LOG_F(INFO, "Waiting for %s's IO threads to end", name); break;
			case 80:
				LOG_F(WARNING, "Stopping io_contexts for %s", name);
				io_ctx_data_->stop();
				io_ctx_service_->stop();
				for (std::size_t k = 0; k < io_threads_.size(); k++) {
					if (!io_threads_[k]->joinable()) {
						LOG_F(ERROR, "%s's io thread #%lu still running", name, k);
					}
				}
				break;
			case 100:
				LOG_F(ERROR, "Detaching io_threads for %s", name);
				for (auto &thread : io_threads_) thread->detach();
				return;
			default: break;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(25));
			if (std::all_of(io_threads_.begin(), io_threads_.end(),
					[](const thread_p &thread) { return thread->joinable(); })) {
				for (auto &thread : io_threads_) thread->join();
				DLOG_F(INFO, "All of %s's IO threads were joined succesfully", name);
				break;
			}
		}
	} catch (std::exception &e) {
		LOG_F(WARNING, "Unexpected error during destruction of a stream outlet: %s", e.what());
	} catch (...) { LOG_F(ERROR, "Severe error during stream outlet shutdown."); }
}

void stream_outlet_impl::push_numeric_raw(const void *data, double timestamp, bool pushthrough) {
	if (lsl::api_config::get_instance()->force_default_timestamps()) timestamp = 0.0;
	if (sync_mode_) {
		// Sync path: directly use user's buffer (zero-copy)
		enqueue_sync(asio::const_buffer(data, sample_factory_->datasize()),
			timestamp == 0.0 ? lsl_clock() : timestamp, pushthrough);
	} else {
		// Async path: copy into sample
		sample_p smp(
			sample_factory_->new_sample(timestamp == 0.0 ? lsl_clock() : timestamp, pushthrough));
		smp->assign_untyped(data);
		send_buffer_->push_sample(smp);
	}
}

bool stream_outlet_impl::have_consumers() {
	// Check both async consumers (via send_buffer) and sync consumers (via tcp_server)
	return send_buffer_->have_consumers() || tcp_server_->have_sync_consumers();
}

bool stream_outlet_impl::wait_for_consumers(double timeout) {
	// For sync mode, we need to poll since sync consumers aren't registered with send_buffer
	if (sync_mode_) {
		auto start = std::chrono::steady_clock::now();
		double elapsed = 0.0;
		while (elapsed < timeout) {
			if (tcp_server_->have_sync_consumers()) return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
		}
		return tcp_server_->have_sync_consumers();
	}
	return send_buffer_->wait_for_consumers(timeout);
}

template <class T>
void stream_outlet_impl::enqueue(const T *data, double timestamp, bool pushthrough) {
	if (lsl::api_config::get_instance()->force_default_timestamps()) timestamp = 0.0;
	// Sync mode only for non-string types (strings have variable size, can't do zero-copy)
	if (sync_mode_ && !std::is_same<T, std::string>::value) {
		// Sync path: directly use user's buffer (zero-copy)
		enqueue_sync(asio::const_buffer(data, sample_factory_->datasize()),
			timestamp == 0.0 ? lsl_clock() : timestamp, pushthrough);
	} else {
		// Async path: copy into sample
		sample_p smp(
			sample_factory_->new_sample(timestamp == 0.0 ? lsl_clock() : timestamp, pushthrough));
		smp->assign_typed(data);
		send_buffer_->push_sample(smp);
	}
}

template void stream_outlet_impl::enqueue<char>(const char *data, double, bool);
template void stream_outlet_impl::enqueue<int16_t>(const int16_t *data, double, bool);
template void stream_outlet_impl::enqueue<int32_t>(const int32_t *data, double, bool);
template void stream_outlet_impl::enqueue<int64_t>(const int64_t *data, double, bool);
template void stream_outlet_impl::enqueue<float>(const float *data, double, bool);
template void stream_outlet_impl::enqueue<double>(const double *data, double, bool);
template void stream_outlet_impl::enqueue<std::string>(const std::string *data, double, bool);

// === Sync mode implementation ===

void stream_outlet_impl::push_timestamp_sync(double timestamp) {
	// Allocate storage for timestamp tag + value in sync_timestamps_
	if (timestamp == DEDUCED_TIMESTAMP) {
		// Deduced timestamp: just send the 1-byte tag, no timestamp value
		sync_timestamps_.emplace_back(TAG_DEDUCED_TIMESTAMP, 0.0);
		auto &ts_entry = sync_timestamps_.back();
		sync_buffers_.push_back(asio::const_buffer(&ts_entry.first, 1));  // tag byte only
	} else {
		// Explicit timestamp: send tag + 8-byte timestamp value
		sync_timestamps_.emplace_back(TAG_TRANSMITTED_TIMESTAMP, timestamp);
		auto &ts_entry = sync_timestamps_.back();
		sync_buffers_.push_back(asio::const_buffer(&ts_entry.first, 1));  // tag byte
		sync_buffers_.push_back(asio::const_buffer(&ts_entry.second, sizeof(double)));
	}
}

void stream_outlet_impl::flush_sync() {
	if (sync_buffers_.empty()) return;
	// Write all buffers to connected consumers
	tcp_server_->write_all_blocking(sync_buffers_);
	// Clear buffers and timestamp storage for next batch
	sync_buffers_.clear();
	sync_timestamps_.clear();
}

void stream_outlet_impl::enqueue_sync(asio::const_buffer buf, double timestamp, bool pushthrough) {
	// Add timestamp
	push_timestamp_sync(timestamp);
	// Add sample data buffer (points to user's buffer - zero copy!)
	sync_buffers_.push_back(buf);
	// Flush if pushthrough is requested
	if (pushthrough) {
		flush_sync();
	}
}

template <class T>
void stream_outlet_impl::enqueue_chunk_sync(
	const T *data, std::size_t num_samples, double timestamp, bool pushthrough) {
	if (lsl::api_config::get_instance()->force_default_timestamps()) timestamp = 0.0;
	if (timestamp == 0.0) timestamp = lsl_clock();

	std::size_t sample_bytes = sample_factory_->datasize();
	std::size_t num_chans = info_->channel_count();

	// Reserve space upfront to avoid reallocations in sync_buffers_
	// Each sample needs: 1-2 buffers for timestamp + 1 buffer for data
	// Note: sync_timestamps_ is a deque (no reserve) to ensure pointer stability
	sync_buffers_.reserve(sync_buffers_.size() + num_samples * 3);

	// First sample: explicit timestamp
	push_timestamp_sync(timestamp);
	sync_buffers_.push_back(asio::const_buffer(data, sample_bytes));

	// Remaining samples: deduced timestamps, contiguous data
	for (std::size_t k = 1; k < num_samples; k++) {
		push_timestamp_sync(DEDUCED_TIMESTAMP);
		sync_buffers_.push_back(
			asio::const_buffer(data + k * num_chans, sample_bytes));
	}

	if (pushthrough) flush_sync();
}

// Explicit template instantiations for enqueue_chunk_sync
template void stream_outlet_impl::enqueue_chunk_sync<char>(
	const char *, std::size_t, double, bool);
template void stream_outlet_impl::enqueue_chunk_sync<int16_t>(
	const int16_t *, std::size_t, double, bool);
template void stream_outlet_impl::enqueue_chunk_sync<int32_t>(
	const int32_t *, std::size_t, double, bool);
template void stream_outlet_impl::enqueue_chunk_sync<int64_t>(
	const int64_t *, std::size_t, double, bool);
template void stream_outlet_impl::enqueue_chunk_sync<float>(
	const float *, std::size_t, double, bool);
template void stream_outlet_impl::enqueue_chunk_sync<double>(
	const double *, std::size_t, double, bool);

} // namespace lsl
