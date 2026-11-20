#include "time_receiver.h"
#include "api_config.h"
#include "inlet_connection.h"
#include "netinterfaces.h"
#include "socket_utils.h"
#include <asio/io_context.hpp>
#include <chrono>
#include <exception>
#include <limits>
#include <loguru.hpp>
#include <memory>
#include <sstream>
#include <string>

/// internally used constant to represent an unassigned time offset
const double NOT_ASSIGNED = std::numeric_limits<double>::max();

using namespace lsl;

/// Whether addr is loopback or one of this machine's interface addresses
static bool is_local_address(const asio::ip::address &addr) {
	if (addr.is_loopback()) return true;
	// ponytail: get_local_interfaces() lists only up, multicast-capable interfaces (and on Windows
	// one address per family per adapter); a missed local address just disables the fallback
	for (const auto &netif : get_local_interfaces())
		if (netif.addr.is_v6() && addr.is_v6()
				? netif.addr.to_v6().to_bytes() == addr.to_v6().to_bytes() // ignore scope id
				: netif.addr == addr)
			return true;
	return false;
}

/// same-clock fallback for tokenless outlets, only for outlets on this machine
static same_clock_fallback make_fallback(const api_config *cfg, const udp::endpoint &outlet) {
	const int rounds = cfg->time_same_clock_fallback_rounds();
	return same_clock_fallback(cfg->time_same_clock_detection() && rounds > 0 &&
									   is_local_address(outlet.address())
								   ? rounds
								   : 0);
}

time_receiver::time_receiver(inlet_connection &conn)
	: conn_(conn), was_reset_(false), timeoffset_(std::numeric_limits<double>::max()),
	  remote_time_(std::numeric_limits<double>::max()),
	  uncertainty_(std::numeric_limits<double>::max()), cfg_(api_config::get_instance()),
	  time_sock_(time_io_), outlet_addr_(conn_.get_udp_endpoint()), next_estimate_(time_io_),
	  aggregate_results_(time_io_), next_packet_(time_io_) {
	conn_.register_onlost(this, &timeoffset_upd_);
	conn_.register_onrecover(this, [this]() {
		reset_timeoffset_on_recovery();
		outlet_addr_ = conn_.get_udp_endpoint();
		fallback_ = make_fallback(cfg_, outlet_addr_);
		DLOG_F(INFO, "Set new time service address: %s", outlet_addr_.address().to_string().c_str());
		// handle outlet switching between IPv4 and IPv6
		time_sock_.close();
		time_sock_.open(outlet_addr_.protocol());
	});
	time_sock_.open(outlet_addr_.protocol());
	fallback_ = make_fallback(cfg_, outlet_addr_);
}

time_receiver::~time_receiver() {
	try {
		conn_.unregister_onrecover(this);
		conn_.unregister_onlost(this);
		time_io_.stop();
		if (time_thread_.joinable()) time_thread_.join();
	} catch (std::exception &e) {
		LOG_F(ERROR, "Unexpected error during destruction of a time_receiver: %s", e.what());
	} catch (...) { LOG_F(ERROR, "Severe error during time receiver shutdown."); }
}


// === external data acccess ===

double time_receiver::time_correction(double timeout) {
	double remote_time, uncertainty;
	return time_correction(&remote_time, &uncertainty, timeout);
}

double time_receiver::time_correction(double *remote_time, double *uncertainty, double timeout) {
	std::unique_lock<std::mutex> lock(timeoffset_mut_);
	auto timeoffset_available = [this]() {
		return (timeoffset_ != std::numeric_limits<double>::max()) || conn_.lost();
	};
	if (!timeoffset_available()) {
		// start thread if not yet running
		if (!time_thread_.joinable()) time_thread_ = std::thread(&time_receiver::time_thread, this);
		// wait until the timeoffset becomes available (or we time out)
		if (timeout >= FOREVER)
			timeoffset_upd_.wait(lock, timeoffset_available);
		else if (!timeoffset_upd_.wait_for(
					 lock, std::chrono::duration<double>(timeout), timeoffset_available))
			throw timeout_error("The time_correction() operation timed out.");
	}
	if (conn_.lost())
		throw lost_error("The stream read by this inlet has been lost. To recover, you need to "
						 "re-resolve the source and re-create the inlet.");
	*remote_time = remote_time_;
	*uncertainty = uncertainty_;
	return timeoffset_;
}

bool time_receiver::was_reset() {
	std::unique_lock<std::mutex> lock(timeoffset_mut_);
	bool result = was_reset_;
	was_reset_ = false;
	return result;
}

// === internal processing ===

void time_receiver::time_thread() {
	conn_.acquire_watchdog();
	loguru::set_thread_name((std::string("T_") += conn_.type_info().name()).c_str());
	DLOG_F(2, "Started time receiver thread");
	try {
		// start an async time estimation
		start_time_estimation();
		// start the IO object (will keep running until cancelled)
		while (true) {
			try {
				time_io_.run();
				break;
			} catch (std::exception &e) {
				LOG_F(WARNING, "Hiccup during time_thread io_context processing: %s", e.what());
			}
		}
	} catch (std::exception &e) {
		LOG_F(WARNING, "time_thread failed unexpectedly with message: %s", e.what());
	}
	conn_.release_watchdog();
}

void time_receiver::start_time_estimation() {
	// clear the estimates buffer
	estimates_.clear();
	estimate_times_.clear();
	fallback_.start_round();
	// generate a new wave id so that we don't confuse packets from earlier (or mis-guided)
	// estimations
	current_wave_id_ = std::rand();
	// start the packet exchange chains
	send_next_packet(1);
	receive_next_packet();
	// schedule the aggregation of results (by the time when all replies should have been received)
	aggregate_results_.expires_after(timeout_sec(
		cfg_->time_probe_max_rtt() + cfg_->time_probe_interval() * cfg_->time_probe_count()));
	aggregate_results_.async_wait([this](err_t err) { result_aggregation_scheduled(err); });
	// schedule the next estimation step
	next_estimate_.expires_after(timeout_sec(cfg_->time_update_interval()));
	next_estimate_.async_wait([this](err_t err) {
		if (err != asio::error::operation_aborted) start_time_estimation();
	});
}

void time_receiver::send_next_packet(int packet_num) {
	try {
		// form the request & send it
		std::ostringstream request;
		request.precision(16);
		request << "LSL:timedata\r\n" << current_wave_id_ << " " << lsl_clock() << "\r\n";
		auto msg_buffer = std::make_shared<std::string>(request.str());
		time_sock_.async_send_to(asio::buffer(*msg_buffer), outlet_addr_,
			[msg_buffer](err_t /*unused*/, std::size_t /*unused*/) {
				/* Do nothing, but keep the msg_buffer alive until async_send is completed */
			});
	} catch (std::exception &e) {
		LOG_F(WARNING, "Error trying to send a time packet: %s", e.what());
	}
	// schedule next packet
	if (packet_num < cfg_->time_probe_count()) {
		next_packet_.expires_after(timeout_sec(cfg_->time_probe_interval()));
		next_packet_.async_wait([this, packet_num](err_t err) {
			if (!err) send_next_packet(packet_num + 1);
		});
	}
}

void time_receiver::receive_next_packet() {
	time_sock_.async_receive_from(asio::buffer(recv_buffer_), remote_endpoint_,
		[this](err_t err, std::size_t len) { handle_receive_outcome(err, len); });
}

void time_receiver::handle_receive_outcome(err_t err, std::size_t len) {
	try {
		if (!err) {
			// parse the buffer contents
			std::istringstream is(std::string(recv_buffer_, len));
			int wave_id;
			is >> wave_id;
			if (wave_id == current_wave_id_) {
				double t0, t1, t2, t3 = lsl_clock();
				std::string clock_id;
				is >> t0 >> t1 >> t2 >> clock_id;
				// calculate RTT and offset
				double rtt =
					(t3 - t0) - (t2 - t1); // round trip time (time passed here - time passed there)
				double offset =
					((t1 - t0) + (t2 - t3)) /
					2; // averaged clock offset (other clock - my clock) with rtt bias averaged out
				// same clock (same OS instance and boot): the offset is exactly 0. The causality
				// check rejects false matches (e.g. cloned VMs) whose clocks differ by more than
				// the one-way delay
				if (cfg_->time_same_clock_detection() && !clock_id.empty() &&
					clock_id == lsl_clock_id() && t0 <= t1 && t2 <= t3)
					rtt = offset = 0;
				fallback_.add_probe(!clock_id.empty(), t0, t1, t2, t3);
				// store it
				estimates_.emplace_back(rtt, offset);
				// local_time, remote_time
				estimate_times_.emplace_back((t3 + t0) / 2.0, (t2 + t1) / 2.0);
			}
		}
	} catch (std::exception &e) {
		LOG_F(WARNING, "Error while processing a time estimation return packet: %s", e.what());
	}
	if (err != asio::error::operation_aborted) receive_next_packet();
}

void time_receiver::result_aggregation_scheduled(err_t err) {
	if (err) return;
	const bool same_clock = fallback_.end_round();

	if ((int)estimates_.size() >= cfg_->time_update_minprobes()) {
		// take the estimate with the lowest error bound (=rtt), as in NTP
		double best_offset = 0, best_rtt = FOREVER;
		double best_remote_time = 0;
		for (std::size_t k = 0; k < estimates_.size(); k++) {
			if (estimates_[k].first < best_rtt) {
				best_rtt = estimates_[k].first;
				best_offset = estimates_[k].second;
				best_remote_time = estimate_times_[k].second;
			}
		}
		// tokenless outlet on this machine consistent with offset 0 for several rounds: report 0.
		// The uncertainty stays best_rtt, the width of the interval the true offset is known to
		// lie in, which is also how far off 0 could be if the clocks were not the same after all
		if (same_clock) best_offset = 0;
		// and notify that the result is available
		{
			std::lock_guard<std::mutex> lock(timeoffset_mut_);
			uncertainty_ = best_rtt;
			timeoffset_ = 0.0 - best_offset; // not -best_offset, which turns 0.0 into -0.0
			remote_time_ = best_remote_time;
		}
		timeoffset_upd_.notify_all();
	}
}

void time_receiver::reset_timeoffset_on_recovery() {
	std::lock_guard<std::mutex> lock(timeoffset_mut_);
	if (timeoffset_ != NOT_ASSIGNED)
		// this will only be set to true if the reset may have caused a possible interruption in the
		// obtained time offsets
		was_reset_ = true;
	timeoffset_ = NOT_ASSIGNED;
}
