#include "inlet_connection.h"
#include "api_config.h"
#include "resolver_impl.h"
#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/basic_resolver.hpp>
#include <chrono>
#include <exception>
#include <functional>
#include <loguru.hpp>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace lsl;
namespace ip = asio::ip;
inlet_connection::inlet_connection(const stream_info_impl &info, bool recover)
	: type_info_(info), host_info_(info), tcp_protocol_(tcp::v4()), udp_protocol_(udp::v4()),
	  recovery_enabled_(recover), lost_(false), shutdown_(false), last_receive_time_(lsl_clock()),
	  active_transmissions_(0) {
	// if the given stream_info is already fully resolved...
	if (!host_info_.v4address().empty() || !host_info_.v6address().empty()) {
		// check LSL protocol version (we strictly forbid incompatible protocols instead of risking
		// silent failure)
		if (type_info_.version() / 100 > api_config::get_instance()->use_protocol_version() / 100)
			throw std::runtime_error(
				(std::string("The received stream (") += host_info_.name()) +=
				") uses a newer protocol version than this inlet. Please update.");

		// select TCP/UDP protocol versions
		if (api_config::get_instance()->allow_ipv6()) {
			// if IPv6 is optionally allowed...
			if (host_info_.v4address().empty() || !host_info_.v4data_port() ||
				!host_info_.v4service_port()) {
				// then use it but only iff there are problems with the IPv4 connection data
				tcp_protocol_ = tcp::v6();
				udp_protocol_ = udp::v6();
			} else {
				// (otherwise stick to IPv4)
				tcp_protocol_ = tcp::v4();
				udp_protocol_ = udp::v4();
			}
		} else {
			// otherwise use the protocol type that is selected in the config
			tcp_protocol_ = api_config::get_instance()->allow_ipv4() ? tcp::v4() : tcp::v6();
			udp_protocol_ = api_config::get_instance()->allow_ipv4() ? udp::v4() : udp::v6();
		}

		if (recovery_enabled_ && type_info_.source_id().empty()) {
			// we cannot correctly recover streams which don't have a unique source id
			LOG_F(WARNING,
				"The stream named '%s' can't be recovered automatically if its provider crashes "
				"because it doesn't have a unique source ID",
				host_info_.name().c_str());
			recovery_enabled_ = false;
		}

	} else {
		// the actual endpoint is not known yet -- we need to discover it later on the fly
		// check that all the necessary information for this has been fully specified
		if (type_info_.name().empty() && type_info_.type().empty() &&
			type_info_.source_id().empty())
			throw std::invalid_argument(
				"When creating an inlet with a constructed (instead of resolved) stream_info, you "
				"must assign at least the name, type or source_id of the desired stream.");
		if (type_info_.channel_count() == 0)
			throw std::invalid_argument(
				"When creating an inlet with a constructed (instead of resolved) stream_info, you "
				"must assign a nonzero channel count.");
		if (type_info_.channel_format() == cft_undefined)
			throw std::invalid_argument("When creating an inlet with a constructed (instead of "
										"resolved) stream_info, you must assign a channel format.");

		// use the protocol that is specified in the config
		tcp_protocol_ = api_config::get_instance()->allow_ipv4() ? tcp::v4() : tcp::v6();
		udp_protocol_ = api_config::get_instance()->allow_ipv4() ? udp::v4() : udp::v6();

		// assign initial dummy endpoints
		host_info_.v4address("127.0.0.1");
		host_info_.v6address("::1");
		host_info_.v4data_port(49999);
		host_info_.v4service_port(49999);
		host_info_.v6data_port(49999);
		host_info_.v6service_port(49999);

		// recovery must generally be enabled
		recovery_enabled_ = true;
	}
}

void inlet_connection::engage() {
	if (recovery_enabled_) watchdog_thread_ = std::thread(&inlet_connection::watchdog_thread, this);
}

void inlet_connection::disengage() {
	// shut down the connection
	{
		std::lock_guard<std::mutex> lock(shutdown_mut_);
		shutdown_ = true;
	}
	shutdown_cond_.notify_all();
	// cancel all operations (resolver, streams, ...)
	resolver_.cancel();
	cancel_and_shutdown();
	// and wait for the watchdog to finish
	if (recovery_enabled_) watchdog_thread_.join();
}


// === external accessors for connection properties ===

/// convert a IPv6 address or hostname into an non-link-local address
ip::address resolve_v6_addr(const std::string &addr) {
	// Try to parse the IPv6 address
	asio::error_code ec;
	auto v6addr = ip::make_address_v6(addr, ec);
	if (!ec && !v6addr.is_link_local()) return v6addr;

	// This more complicated procedure is required when the address is an ipv6 link-local address.
	// Simplified from https://stackoverflow.com/a/10303761/73299
	asio::io_context io;
	auto res = ip::tcp::resolver(io).resolve(addr, "");
	if (res.empty()) throw lost_error("Unable to resolve tcp stream at address: " + addr);
	return res.begin()->endpoint().address();
}

tcp::endpoint inlet_connection::get_tcp_endpoint() {
	shared_lock_t lock(host_info_mut_);
	if (tcp_protocol_ == tcp::v4())
		return {ip::make_address(host_info_.v4address()), host_info_.v4data_port()};

	std::string addr = host_info_.v6address();
	uint16_t port = host_info_.v6data_port();
	lock.unlock();
	return {resolve_v6_addr(addr), port};
}

udp::endpoint inlet_connection::get_udp_endpoint() {
	shared_lock_t lock(host_info_mut_);
	if (udp_protocol_ == udp::v4())
		return {ip::make_address(host_info_.v4address()), host_info_.v4service_port()};

	std::string addr = host_info_.v6address();
	uint16_t port = host_info_.v6service_port();
	lock.unlock();
	return {resolve_v6_addr(addr), port};
}

std::string inlet_connection::current_uid() {
	shared_lock_t lock(host_info_mut_);
	return host_info_.uid();
}

double inlet_connection::current_srate() {
	shared_lock_t lock(host_info_mut_);
	return host_info_.nominal_srate();
}


// === connection recovery logic ===
void inlet_connection::try_recover() {
	if (recovery_enabled_) {
		try {
			std::lock_guard<std::mutex> lock(recovery_mut_);
			// first create the query string based on the known stream information
			std::ostringstream query;
			{
				shared_lock_t lock(host_info_mut_);
				// construct query according to the fields that are present in the stream_info
				const char *channel_format_strings[] = {"undefined", "float32", "double64",
					"string", "int32", "int16", "int8", "int64"};
				query << "channel_count='" << host_info_.channel_count() << "'";
				if (!host_info_.name().empty()) query << " and name='" << host_info_.name() << "'";
				if (!host_info_.type().empty()) query << " and type='" << host_info_.type() << "'";
				// for floating point values, str2double(double2str(fpvalue)) == fpvalue is most
				// likely wrong and might lead to streams not being resolved.
				// We accept that a lost stream might be replaced by a stream from the same host
				// with the same type, channel type and channel count but a different srate
				/*if (host_info_.nominal_srate() > 0)
					query << " and nominal_srate='" << host_info_.nominal_srate() << "'";
					*/
				if (!host_info_.source_id().empty())
					query << " and source_id='" << host_info_.source_id() << "'";
				query << " and channel_format='"
					  << channel_format_strings[host_info_.channel_format()] << "'";
			}
			// attempt a recovery
			for (int attempt = 0;; attempt++) {
				// issue the resolve (blocks until it is either cancelled or got at least one
				// matching streaminfo and has waited for a certain timeout)
				std::vector<stream_info_impl> infos =
					resolver_.resolve_oneshot(query.str(), 1, FOREVER, attempt == 0 ? 1.0 : 5.0);
				if (!infos.empty()) {
					// got a result
					unique_lock_t lock(host_info_mut_);
					// check if any of the returned streams is the one that we're currently
					// connected to
					for (auto &info : infos)
						if (info.uid() == host_info_.uid())
							return; // in this case there is no need to recover (we're still fine)
					// otherwise our stream is gone and we indeed need to recover:
					// ensure that the query result is unique (since someone might have used a
					// non-unique stream ID)
					if (infos.size() == 1) {
						// update the endpoint
						host_info_ = infos[0];
						// cancel all cancellable operations registered with this connection
						cancel_all_registered();
						// invoke any callbacks associated with a connection recovery
						std::lock_guard<std::mutex> lock(onrecover_mut_);
						for (auto &pair : onrecover_) (pair.second)();
					} else {
						// there are multiple possible streams to connect to in a recovery attempt:
						// we warn and re-try this is because we don't want to randomly connect to
						// the wrong source without the user knowing about it; the correct action
						// (if this stream shall indeed have multiple instances) is to change the
						// user code and make its source_id unique, or remove the source_id
						// altogether if that's not possible (therefore disabling the ability to
						// recover)
						LOG_F(WARNING,
							"Found multiple streams with name='%s' and source_id='%s'. "
							"Cannot recover unless all but one are closed.",
							host_info_.name().c_str(), host_info_.source_id().c_str());
						continue;
					}
				} else {
					// cancelled
				}
				break;
			}
		} catch (std::exception &e) {
			LOG_F(ERROR, "A recovery attempt encountered an unexpected error: %s", e.what());
		}
	}
}

void inlet_connection::watchdog_thread() {
	loguru::set_thread_name((std::string("W_") += type_info().name().substr(0, 12)).c_str());
	while (!lost_ && !shutdown_) {
		try {
			// we only try to recover if a) there are active transmissions and b) we haven't seen
			// new data for some time
			{
				std::unique_lock<std::mutex> lock(client_status_mut_);
				if ((active_transmissions_ > 0) &&
					(lsl_clock() - last_receive_time_ >
						api_config::get_instance()->watchdog_time_threshold())) {
					lock.unlock();
					try_recover();
				}
			}
			// instead of sleeping we're waiting on a condition variable for the sleep duration
			// so that the watchdog can be cancelled conveniently
			{
				std::unique_lock<std::mutex> lock(shutdown_mut_);
				shutdown_cond_.wait_for(lock,
					std::chrono::duration<double>(
						api_config::get_instance()->watchdog_check_interval()),
					[this]() { return shutdown(); });
			}
		} catch (std::exception &e) {
			LOG_F(ERROR, "Unexpected hiccup in the watchdog thread: %s", e.what());
		}
	}
}

void inlet_connection::try_recover_from_error() {
	if (!shutdown_) {
		if (!recovery_enabled_) {
			// if the stream is irrecoverable it is now lost,
			// so we need to notify the other inlet components
			lost_ = true;
			try {
				std::lock_guard<std::mutex> lock(client_status_mut_);
				for (auto &pair : onlost_) pair.second->notify_all();
			} catch (std::exception &e) {
				LOG_F(ERROR,
					"Unexpected problem while trying to issue a connection loss notification: %s",
					e.what());
			}
			throw lost_error("The stream read by this inlet has been lost. To recover, you need to "
							 "re-resolve the source and re-create the inlet.");
		} else
			try_recover();
	}
}


// === client status updates ===

void inlet_connection::acquire_watchdog() {
	std::lock_guard<std::mutex> lock(client_status_mut_);
	active_transmissions_++;
}

void inlet_connection::release_watchdog() {
	std::lock_guard<std::mutex> lock(client_status_mut_);
	active_transmissions_--;
}

void inlet_connection::update_receive_time(double t) {
	std::lock_guard<std::mutex> lock(client_status_mut_);
	last_receive_time_ = t;
}

void inlet_connection::register_onlost(void *id, std::condition_variable *cond) {
	std::lock_guard<std::mutex> lock(client_status_mut_);
	onlost_[id] = cond;
}

void inlet_connection::unregister_onlost(void *id) {
	std::lock_guard<std::mutex> lock(client_status_mut_);
	onlost_.erase(id);
}

void inlet_connection::register_onrecover(void *id, const std::function<void()> &func) {
	std::lock_guard<std::mutex> lock(onrecover_mut_);
	onrecover_[id] = func;
}

void inlet_connection::unregister_onrecover(void *id) {
	std::lock_guard<std::mutex> lock(onrecover_mut_);
	onrecover_.erase(id);
}
