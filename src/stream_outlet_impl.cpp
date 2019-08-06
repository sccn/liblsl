#include "stream_outlet_impl.h"
#include "api_config.h"
#include "sample.h"
#include "send_buffer.h"
#include "stream_info_impl.h"
#include "tcp_server.h"
#include "udp_server.h"
#include <algorithm>
#include <chrono>
#include <memory>

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
	  io_ctx_service_(std::make_shared<asio::io_context>(1)) {
	ensure_lsl_initialized();
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
		chunk_size_, cfg->allow_ipv4(), cfg->allow_ipv6());

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
	sample_p smp(
		sample_factory_->new_sample(timestamp == 0.0 ? lsl_clock() : timestamp, pushthrough));
	smp->assign_untyped(data);
	send_buffer_->push_sample(smp);
}

bool stream_outlet_impl::have_consumers() { return send_buffer_->have_consumers(); }

bool stream_outlet_impl::wait_for_consumers(double timeout) {
	return send_buffer_->wait_for_consumers(timeout);
}

template <class T>
void stream_outlet_impl::enqueue(const T *data, double timestamp, bool pushthrough) {
	if (lsl::api_config::get_instance()->force_default_timestamps()) timestamp = 0.0;
	sample_p smp(
		sample_factory_->new_sample(timestamp == 0.0 ? lsl_clock() : timestamp, pushthrough));
	smp->assign_typed(data);
	send_buffer_->push_sample(smp);
}

template void stream_outlet_impl::enqueue<char>(const char *data, double, bool);
template void stream_outlet_impl::enqueue<int16_t>(const int16_t *data, double, bool);
template void stream_outlet_impl::enqueue<int32_t>(const int32_t *data, double, bool);
template void stream_outlet_impl::enqueue<int64_t>(const int64_t *data, double, bool);
template void stream_outlet_impl::enqueue<float>(const float *data, double, bool);
template void stream_outlet_impl::enqueue<double>(const double *data, double, bool);
template void stream_outlet_impl::enqueue<std::string>(const std::string *data, double, bool);

} // namespace lsl
