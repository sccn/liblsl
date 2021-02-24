#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

namespace asio = lslboost::asio;

namespace lsl {
inline asio::chrono::milliseconds timeout_sec(double timeout_seconds) {
	return asio::chrono::milliseconds(static_cast<unsigned int>(1000 * timeout_seconds));
}

/// Bind a socket to a free port in the configured port range or throw an error otherwise.
uint16_t bind_port_in_range(asio::ip::udp::socket &sock, asio::ip::udp protocol);

/// Bind and listen to an acceptor on a free port in the configured port range or throw an error.
uint16_t bind_and_listen_to_port_in_range(
	asio::ip::tcp::acceptor &acc, asio::ip::tcp protocol, int backlog);

/// Measure the endian conversion performance of this machine.
double measure_endian_performance();
} // namespace lsl

#endif
