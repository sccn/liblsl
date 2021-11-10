#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>

using udp_socket = asio::basic_datagram_socket<asio::ip::udp, asio::io_context::executor_type>;
using tcp_socket = asio::basic_stream_socket<asio::ip::tcp, asio::io_context::executor_type>;
using tcp_acceptor = asio::basic_socket_acceptor<asio::ip::tcp, asio::io_context::executor_type>;

namespace lsl {
inline asio::chrono::milliseconds timeout_sec(double timeout_seconds) {
	return asio::chrono::milliseconds(static_cast<unsigned int>(1000 * timeout_seconds));
}

/// Bind a socket to a free port in the configured port range or throw an error otherwise.
uint16_t bind_port_in_range(udp_socket &sock, asio::ip::udp protocol);

/// Bind and listen to an acceptor on a free port in the configured port range or throw an error.
uint16_t bind_and_listen_to_port_in_range(
	tcp_acceptor &acc, asio::ip::tcp protocol, int backlog);
} // namespace lsl

#endif
