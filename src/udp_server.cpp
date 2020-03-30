#include "udp_server.h"
#include "socket_utils.h"
#include "stream_info_impl.h"
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/thread/thread_only.hpp>

using namespace lsl;
using namespace lslboost::asio;
using err_t = const lslboost::system::error_code &;

udp_server::udp_server(const stream_info_impl_p &info, io_context &io, udp protocol)
	: info_(info), io_(io), socket_(std::make_shared<udp::socket>(io)),
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
	LOG_F(2, "%s: Started unicast udp server %p at port %d", info_->name().c_str(), this, port);
}

udp_server::udp_server(const stream_info_impl_p &info, io_context &io, const std::string &address,
	uint16_t port, int ttl, const std::string &listen_address)
	: info_(info), io_(io), socket_(std::make_shared<udp::socket>(io)),
	  time_services_enabled_(false) {
	ip::address addr = ip::make_address(address);
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

	// join the multicast group, if any
	if (addr.is_multicast() && !is_broadcast) {
		if (addr.is_v4())
			socket_->set_option(
				ip::multicast::join_group(addr.to_v4(), listen_endpoint.address().to_v4()));
		else
			socket_->set_option(ip::multicast::join_group(addr));
	}
	LOG_F(2, "%s: Started multicast udp server at %s port %d", this->info_->name().c_str(),
		address.c_str(), port);
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
	post(io_, [sock]() {
		try {
			if (sock->is_open()) sock->close();
		} catch (std::exception &e) { LOG_F(ERROR, "Error during %s: %s", __func__, e.what()); }
	});
}

// === receive / reply loop ===

void udp_server::request_next_packet() {
	DLOG_F(5, "udp_server::request_next_packet");
	socket_->async_receive_from(lslboost::asio::buffer(buffer_), remote_endpoint_,
		[shared_this = shared_from_this()](
			err_t err, std::size_t len) { shared_this->handle_receive_outcome(err, len); });
}

void udp_server::handle_receive_outcome(error_code err, std::size_t len) {
	DLOG_F(6, "udp_server::handle_receive_outcome (%lub)", len);
	if (err != error::operation_aborted && err != error::shut_down) {
		try {
			if (!err) {
				// remember the time of packet reception for possible later use
				double t1 = time_services_enabled_ ? lsl_clock() : 0.0;

				// wrap received packet into a request stream and parse the method from it
				std::istringstream request_stream(std::string(buffer_, buffer_ + len));
				std::string method;
				getline(request_stream, method);
				method = trim(method);
				if (method == "LSL:shortinfo") {
					// shortinfo request: parse content query string
					std::string query;
					getline(request_stream, query);
					query = trim(query);
					// parse return address, port, and query ID
					uint16_t return_port;
					request_stream >> return_port;
					std::string query_id;
					request_stream >> query_id;
					// check query
					if (info_->matches_query(query)) {
						// query matches: send back reply
						udp::endpoint return_endpoint(remote_endpoint_.address(), return_port);
						string_p replymsg(
							std::make_shared<std::string>((query_id += "\r\n") += shortinfo_msg_));
						socket_->async_send_to(lslboost::asio::buffer(*replymsg), return_endpoint,
							[shared_this = shared_from_this(), replymsg](err_t err_, std::size_t) {
								if (err_ != error::operation_aborted && err_ != error::shut_down)
									shared_this->request_next_packet();
							});
						return;
					} else {
						LOG_F(INFO, "%p Got shortinfo query for mismatching query string: %s", this,
							query.c_str());
					}
				} else if (time_services_enabled_ && method == "LSL:timedata") {
					// timedata request: parse time of original transmission
					int wave_id;
					request_stream >> wave_id;
					double t0;
					request_stream >> t0;
					// send it off (including the time of packet submission and a shared ptr to the
					// message content owned by the handler)
					std::ostringstream reply;
					reply.precision(16);
					reply << " " << wave_id << " " << t0 << " " << t1 << " " << lsl_clock();
					string_p replymsg(std::make_shared<std::string>(reply.str()));
					socket_->async_send_to(lslboost::asio::buffer(*replymsg), remote_endpoint_,
						[shared_this = shared_from_this(), replymsg](err_t err_, std::size_t) {
							if (err_ != error::operation_aborted && err_ != error::shut_down)
								shared_this->request_next_packet();
						});
					return;
				} else {
					DLOG_F(INFO, "Unknown method '%s' received by udp-server", method.c_str());
				}
			}
		} catch (std::exception &e) {
			LOG_F(WARNING, "udp_server: hiccup during request processing: %s", e.what());
		}
		request_next_packet();
	}
}
