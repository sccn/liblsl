#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include "forward.h"
#include "socket_utils.h"
#include <asio/ip/udp.hpp>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>

using asio::ip::udp;
using err_t = const asio::error_code &;

namespace lsl {
/// shared pointer to a socket
using udp_socket_p = std::shared_ptr<udp_socket>;

/**
 * A lightweight UDP responder service.
 *
 * Understands the following messages:
 *  - `LSL:shortinfo`. This is a request for the stream_info that comes with a query string (and a
 * return address). A packet is returned only if the query matches.
 *  - `LSL:timedata`. This is a request for time synchronization info that comes with a time stamp
 * (t0). The t0 stamp and two more time stamps (t1 and t2) are returned (similar to the NTP packet
 * exchange).
 */
class udp_server : public std::enable_shared_from_this<udp_server> {
public:
	/**
	 * Create a UDP responder that listens "side by side" with a TCP server.
	 *
	 * This server will listen on a free local port for timedata and shortinfo requests -- mainly
	 * for timing information (unless shortinfo is needed by clients).
	 * @param info The stream_info of the stream to serve (shared). After success, the appropriate
	 * service port will be assigned.
	 * @param io asio::io_context that runs the server's async operations
	 * @param protocol The protocol stack to use (tcp::v4() or tcp::v6()).
	 */
	udp_server(stream_info_impl_p info, asio::io_context &io, udp protocol);

	/**
	 * Create a new UDP server in multicast mode.
	 *
	 * This server will listen on a multicast address and responds only to LSL:shortinfo requests.
	 * This is for multicast/broadcast (and optionally unicast) local service discovery.
	 */
	udp_server(stream_info_impl_p info, asio::io_context &io, asio::ip::address addr,
		uint16_t port, int ttl, const std::string &listen_address);


	/// Start serving UDP traffic.
	/// Call this only after the (shared) info object has been initialized by every involved party.
	void begin_serving();

	/// Initiate teardown of UDP traffic.
	void end_serving();

private:
	/// Initiate next packet request.
	/// The result of the operation will eventually trigger the handle_receive_outcome() handler.
	void request_next_packet();

	/// Handler that gets called when a the next packet was received (or the op was cancelled).
	void handle_receive_outcome(err_t err, std::size_t len);

	/// Parse and process a LSL::shortinfo request
	void process_shortinfo_request(std::istream& request_stream);

	/// Parse and process a LSL::timedata request
	void process_timedata_request(std::istream& request_stream, double t1);

	/// stream_info reference
	stream_info_impl_p info_;
	/// IO service reference
	asio::io_context &io_;
	udp_socket_p socket_;

	/// a buffer of data (we're receiving on it)
	char buffer_[65536]{0};
	bool time_services_enabled_;
	/// the endpoint that we're currently talking to)
	udp::endpoint remote_endpoint_;
	/// pre-computed server response
	std::string shortinfo_msg_;
};
} // namespace lsl

#endif
