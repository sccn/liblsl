#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

using namespace lslboost::asio::ip;

namespace lsl {
inline lslboost::asio::chrono::milliseconds timeout_sec(double timeout_seconds) {
	return lslboost::asio::chrono::milliseconds(static_cast<unsigned int>(1000 * timeout_seconds));
}

/// Bind a socket to a free port in the configured port range or throw an error otherwise.
uint16_t bind_port_in_range(udp::socket &sock, udp protocol);

/// Bind and listen to an acceptor on a free port in the configured port range or throw an error.
uint16_t bind_and_listen_to_port_in_range(tcp::acceptor &acc, tcp protocol, int backlog);

/// Measure the endian conversion performance of this machine.
double measure_endian_performance();
} // namespace lsl

#endif
