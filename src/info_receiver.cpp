#include "info_receiver.h"
#include "cancellable_streambuf.h"
#include "inlet_connection.h"
#include "stream_info_impl.h"
#include <chrono>
#include <exception>
#include <istream>
#include <loguru.hpp>
#include <memory>
#include <sstream>
#include <string>

lsl::info_receiver::info_receiver(inlet_connection &conn) : conn_(conn) {
	conn_.register_onlost(this, &fullinfo_upd_);
}

lsl::info_receiver::~info_receiver() {
	try {
		conn_.unregister_onlost(this);
		if (info_thread_.joinable()) info_thread_.join();
	} catch (std::exception &e) {
		LOG_F(ERROR, "Unexpected error during destruction of an info_receiver: %s", e.what());
	} catch (...) { LOG_F(ERROR, "Severe error during info receiver shutdown."); }
}

const lsl::stream_info_impl &lsl::info_receiver::info(double timeout) {
	std::unique_lock<std::mutex> lock(fullinfo_mut_);
	auto info_ready = [this]() { return fullinfo_ || conn_.lost(); };
	if (!info_ready()) {
		// start thread if not yet running
		if (!info_thread_.joinable()) info_thread_ = std::thread(&info_receiver::info_thread, this);
		// wait until we are ready to return a result (or we time out)
		if (timeout >= FOREVER)
			fullinfo_upd_.wait(lock, info_ready);
		else if (!fullinfo_upd_.wait_for(lock, std::chrono::duration<double>(timeout), info_ready))
			throw timeout_error("The info() operation timed out.");
	}
	if (conn_.lost())
		throw lost_error("The stream read by this inlet has been lost. To recover, you need to "
						 "re-resolve the source and re-create the inlet.");
	return *fullinfo_;
}

void lsl::info_receiver::info_thread() {
	conn_.acquire_watchdog();
	loguru::set_thread_name((std::string("I_") += conn_.type_info().name().substr(0, 12)).c_str());
	try {
		while (!conn_.lost() && !conn_.shutdown()) {
			try {
				// make a new stream buffer & stream
				cancellable_streambuf buffer;
				buffer.register_at(&conn_);
				std::iostream server_stream(&buffer);
				// connect...
				if (nullptr == buffer.connect(conn_.get_tcp_endpoint()))
				{
					throw asio::system_error(buffer.error());
				}
				// send the query
				server_stream << "LSL:fullinfo\r\n" << std::flush;
				// receive and parse the response
				std::ostringstream os;
				os << server_stream.rdbuf();
				stream_info_impl info;
				std::string msg = os.str();
				info.from_fullinfo_message(msg);
				// if this is not a valid streaminfo we retry
				if (info.created_at() == 0.0) continue;
				// store the result for pickup & return
				{
					std::lock_guard<std::mutex> lock(fullinfo_mut_);
					fullinfo_ = std::make_shared<stream_info_impl>(info);
				}
				fullinfo_upd_.notify_all();
				break;
			} catch (err_t) {
				// connection-level error: closed, reset, refused, etc.
				conn_.try_recover_from_error();
			} catch (std::exception &e) {
				// parsing-level error: intermittent disconnect or invalid protocol
				LOG_F(ERROR, "Error while receiving the stream info (%s); retrying...", e.what());
				conn_.try_recover_from_error();
			}
		}
	} catch (lost_error &) {}
	conn_.release_watchdog();
}
