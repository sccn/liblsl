#include "udp_server.h"
#include "api_config.h"
#include "socket_utils.h"
#include "stream_info_impl.h"
#include "util/strfuns.hpp"
#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/multicast.hpp>
#include <asio/ip/udp.hpp>
#include <exception>
#include <loguru.hpp>
#include <sstream>
#include <utility>

namespace ip = asio::ip;

namespace lsl {

udp_server::udp_server(stream_info_impl_p info, asio::io_context &io, udp protocol)
	: info_(std::move(info)), io_(io), socket_(std::make_shared<udp_socket_p::element_type>(io)),
	  time_services_enabled_(true) {
	// open the socket for the specified protocol
	socket_->open(protocol);

	// bind to a free port
	uint16_t port = bind_port_in_range(*socket_, protocol);

	// assign the service port field
	if (protocol == udp::v4())
		info_->v4service_port(port);
	else
		info_->v6service_port(port);
	LOG_F(2, "%s: Started unicast udp server at port %d (addr %p)", info_->name().c_str(), port,
		(void *)this);
}

udp_server::udp_server(stream_info_impl_p info, asio::io_context &io, ip::address addr,
	uint16_t port, int ttl, const std::string &listen_address)
	: info_(std::move(info)), io_(io), socket_(std::make_shared<udp_socket>(io)),
	  time_services_enabled_(false) {
	bool is_broadcast = addr == ip::address_v4::broadcast();

	// set up the endpoint where we listen (note: this is not yet the multicast address)
	udp::endpoint listen_endpoint;
	if (listen_address.empty()) {
		// pick the default endpoint
		if (addr.is_v4())
			listen_endpoint = udp::endpoint(udp::v4(), port);
		else
			listen_endpoint = udp::endpoint(udp::v6(), port);
	} else {
		// choose an endpoint explicitly
		ip::address listen_addr = ip::make_address(listen_address);
		listen_endpoint = udp::endpoint(listen_addr, (uint16_t)port);
	}

	// open the socket and make sure that we can reuse the address, and bind it
	socket_->open(listen_endpoint.protocol());
	socket_->set_option(udp::socket::reuse_address(true));

	// set the multicast TTL
	if (addr.is_multicast() && !is_broadcast) socket_->set_option(ip::multicast::hops(ttl));

	// bind to the listen endpoint
	socket_->bind(listen_endpoint);

	// join the multicast groups
	if (addr.is_multicast() && !is_broadcast) {
		bool joined_anywhere = false;
		asio::error_code err;
		for (auto &if_ : api_config::get_instance()->multicast_interfaces) {
			DLOG_F(
				INFO, "Joining %s to %s", if_.addr.to_string().c_str(), addr.to_string().c_str());
			if (addr.is_v4() && if_.addr.is_v4())
				socket_->set_option(ip::multicast::join_group(addr.to_v4(), if_.addr.to_v4()), err);
			else if (addr.is_v6() && if_.addr.is_v6())
				socket_->set_option(
					ip::multicast::join_group(addr.to_v6(), if_.addr.to_v6().scope_id()), err);
			if (err)
				LOG_F(WARNING, "Could not bind multicast responder for %s to interface %s (%s)",
					addr.to_string().c_str(), if_.addr.to_string().c_str(), err.message().c_str());
			else
				joined_anywhere = true;
		}
		if (!joined_anywhere) throw std::runtime_error("Could not join any multicast group");
	}
	LOG_F(2, "%s: Started multicast udp server at %s port %d (addr %p)",
		this->info_->name().c_str(), addr.to_string().c_str(), port, (void *)this);
}

// === externally issued asynchronous commands ===

void udp_server::begin_serving() {
	// pre-calculate the shortinfo message (now that everyone should have initialized their part).
	shortinfo_msg_ = info_->to_shortinfo_message();
	// start asking for a packet
	request_next_packet();
}

void udp_server::end_serving() {
	// gracefully close the socket; this will eventually lead to the cancellation of the IO
	// operation(s) tied to its socket
	auto sock(socket_); // socket shared ptr to be kept alive
	const char *fn = __func__;
	post(io_, [sock, fn]() {
		try {
			if (sock->is_open()) sock->close();
		} catch (std::exception &e) { LOG_F(ERROR, "Error during %s: %s", fn, e.what()); }
	});
}

// === receive / reply loop ===

void udp_server::request_next_packet() {
	DLOG_F(5, "udp_server::request_next_packet");
	socket_->async_receive_from(asio::buffer(buffer_), remote_endpoint_,
		[shared_this = shared_from_this()](
			err_t err, std::size_t len) { shared_this->handle_receive_outcome(err, len); });
}

void udp_server::process_shortinfo_request(std::istream& request_stream)
{
	std::string query;
	getline(request_stream, query);
	query = trim(query);
	// parse return address, port, and query ID
	uint16_t return_port;
	request_stream >> return_port;
	std::string query_id;
	request_stream >> query_id;
	DLOG_F(2, "%p shortinfo req from %s for %s", (void *)this,
		remote_endpoint_.address().to_string().c_str(), query.c_str());
	// check query
	if (info_->matches_query(query)) {
		LOG_F(3, "%p query matches, replying to port %d", (void *)this, return_port);
		// query matches: send back reply
		udp::endpoint return_endpoint(remote_endpoint_.address(), return_port);
		string_p replymsg(
			std::make_shared<std::string>((query_id += "\r\n") += shortinfo_msg_));
		socket_->async_send_to(asio::buffer(*replymsg), return_endpoint,
			[shared_this = shared_from_this(), replymsg](err_t err_, std::size_t /*unused*/) {
				if (err_ != asio::error::operation_aborted && err_ != asio::error::shut_down)
					shared_this->request_next_packet();
			});
	} else {
		DLOG_F(2, "%p query didn't match", (void *)this);
		request_next_packet();
	}
}

void udp_server::process_timedata_request(std::istream &request_stream, double t1) {
	int wave_id;
	request_stream >> wave_id;
	double t0;
	request_stream >> t0;
	// send it off (including the time of packet submission and a shared ptr to the
	// message content owned by the handler)
	std::ostringstream reply;
	reply.precision(16);
	reply << ' ' << wave_id << ' ' << t0 << ' ' << t1 << ' ' << lsl_clock();
	string_p replymsg(std::make_shared<std::string>(reply.str()));
	socket_->async_send_to(asio::buffer(*replymsg), remote_endpoint_,
		[shared_this = shared_from_this(), replymsg](err_t err_, std::size_t /*unused*/) {
			if (err_ != asio::error::operation_aborted && err_ != asio::error::shut_down)
				shared_this->request_next_packet();
		});
}

void udp_server::handle_receive_outcome(err_t err, std::size_t len) {
	DLOG_F(6, "udp_server::handle_receive_outcome (%lub)", len);
	if (err) {
		// non-critical error? Wait for the next packet
		if (err != asio::error::operation_aborted || err != asio::error::shut_down)
			request_next_packet();
		return;
	}
	try {
		// remember the time of packet reception for possible later use
		double t1 = time_services_enabled_ ? lsl_clock() : 0.0;

		// wrap received packet into a request stream and parse the method from it
		std::istringstream request_stream(std::string(buffer_, buffer_ + len));
		std::string method;
		getline(request_stream, method);
		method = trim(method);
		if (method == "LSL:shortinfo") {
			// shortinfo request: parse content query string
			process_shortinfo_request(request_stream);
			return;
		}
		if (time_services_enabled_ && method == "LSL:timedata") {
			// timedata request: parse time of original transmission
			process_timedata_request(request_stream, t1);
			return;
		}
		DLOG_F(INFO, "%p Unknown method '%s' received by udp-server", (void *)this, method.c_str());
	} catch (std::exception &e) {
		LOG_F(
			WARNING, "%p udp_server: hiccup during request processing: %s", (void *)this, e.what());
	}
	request_next_packet();
}
} // namespace lsl
