#include "stream_outlet_impl.h"
#include "tcp_server.h"
#include "udp_server.h"
#include <boost/thread/thread_only.hpp>
#include <memory>

// === implementation of the stream_outlet_impl class ===

using namespace lsl;
using namespace lslboost::asio;

stream_outlet_impl::stream_outlet_impl(
	const stream_info_impl &info, int chunk_size, int max_capacity)
	: chunk_size_(chunk_size), info_(std::make_shared<stream_info_impl>(info)),
	  sample_factory_(std::make_shared<factory>(info.channel_format(), info.channel_count(),
		  static_cast<uint32_t>(
			  info.nominal_srate()
				  ? info.nominal_srate() * api_config::get_instance()->outlet_buffer_reserve_ms() /
						1000
				  : api_config::get_instance()->outlet_buffer_reserve_samples()))),
	  send_buffer_(std::make_shared<send_buffer>(max_capacity)) {
	ensure_lsl_initialized();
	const api_config *cfg = api_config::get_instance();

	// instantiate IPv4 and/or IPv6 stacks (depending on settings)
	if (cfg->allow_ipv4()) try {
			instantiate_stack(tcp::v4(), udp::v4());
		} catch (std::exception &e) {
			LOG_F(WARNING, "Could not instantiate IPv4 stack: %s", e.what());
		}
	if (cfg->allow_ipv6()) try {
			instantiate_stack(tcp::v6(), udp::v6());
		} catch (std::exception &e) {
			LOG_F(WARNING, "Could not instantiate IPv6 stack: %s", e.what());
		}

	// fail if both stacks failed to instantiate
	if (tcp_servers_.empty() || udp_servers_.empty())
		throw std::runtime_error("Neither the IPv4 nor the IPv6 stack could be instantiated.");

	// get the async request chains set up
	for (auto &tcp_server : tcp_servers_) tcp_server->begin_serving();
	for (auto &udp_server : udp_servers_) udp_server->begin_serving();
	for (auto &responder : responders_) responder->begin_serving();

	// and start the IO threads to handle them
	const std::string name{"IO_" + this->info().name().substr(0, 11)};
	for (const auto &io : ios_)
		io_threads_.emplace_back(std::make_shared<lslboost::thread>([io, name]() {
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

void stream_outlet_impl::instantiate_stack(tcp tcp_protocol, udp udp_protocol) {
	// get api_config
	const api_config *cfg = api_config::get_instance();
	std::string listen_address = cfg->listen_address();
	std::vector<std::string> multicast_addrs = cfg->multicast_addresses();
	int multicast_ttl = cfg->multicast_ttl();
	uint16_t multicast_port = cfg->multicast_port();
	LOG_F(2, "%s: Trying to listen at address '%s'", info().name().c_str(), listen_address.c_str());
	// create TCP data server
	ios_.push_back(std::make_shared<io_context>());
	tcp_servers_.push_back(std::make_shared<tcp_server>(
		info_, ios_.back(), send_buffer_, sample_factory_, tcp_protocol, chunk_size_));
	// create UDP time server
	ios_.push_back(std::make_shared<io_context>());
	udp_servers_.push_back(std::make_shared<udp_server>(info_, *ios_.back(), udp_protocol));
	// create UDP multicast responders
	for (const auto &mcastaddr : multicast_addrs) {
		try {
			// use only addresses for the protocol that we're supposed to use here
			ip::address address(ip::make_address(mcastaddr));
			if (udp_protocol == udp::v4() ? address.is_v4() : address.is_v6())
				responders_.push_back(std::make_shared<udp_server>(
					info_, *ios_.back(), mcastaddr, multicast_port, multicast_ttl, listen_address));
		} catch (std::exception &e) {
			LOG_F(WARNING, "Couldn't create multicast responder for %s (%s)", mcastaddr.c_str(),
				e.what());
		}
	}
}

stream_outlet_impl::~stream_outlet_impl() {
	try {
		// cancel all request chains
		for (auto &tcp_server : tcp_servers_) tcp_server->end_serving();
		for (auto &udp_server : udp_servers_) udp_server->end_serving();
		for (auto &responder : responders_) responder->end_serving();
		// join the IO threads
		for (std::size_t k = 0; k < io_threads_.size(); k++)
			if (!io_threads_[k]->try_join_for(lslboost::chrono::milliseconds(1000))) {
				// .. using force, if necessary (should only ever happen if the CPU is maxed out)
				std::ostringstream os;
				os << io_threads_[k]->get_id();
				LOG_F(ERROR, "Tearing down stream_outlet of thread %s", os.str().c_str());
				ios_[k]->stop();
				for (int attempt = 1;
					 !io_threads_[k]->try_join_for(lslboost::chrono::milliseconds(1000));
					 attempt++) {
					LOG_F(ERROR, "Thread %s didn't complete.", os.str().c_str());
					io_threads_[k]->interrupt();
				}
			}
	} catch (std::exception &e) {
		LOG_F(WARNING, "Unexpected error during destruction of a stream outlet: %s", e.what());
	} catch (...) { LOG_F(ERROR, "Severe error during stream outlet shutdown."); }
}

bool stream_outlet_impl::have_consumers() { return send_buffer_->have_consumers(); }

bool stream_outlet_impl::wait_for_consumers(double timeout) {
	return send_buffer_->wait_for_consumers(timeout);
}
