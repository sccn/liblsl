#include "socket_utils.h"
#include "api_config.h"
#include "common.h"
#include <loguru.hpp>
#ifdef _WIN32
#include <mstcpip.h>
#endif

template <typename Socket, typename Protocol>
uint16_t bind_port_in_range_(Socket &sock, Protocol protocol) {
	const auto *cfg = lsl::api_config::get_instance();
	asio::error_code ec;
	for (uint16_t port = cfg->base_port(), e = port + cfg->port_range(); port < e; port++) {
		sock.bind(typename Protocol::endpoint(protocol, port), ec);
		if (ec == asio::error::address_in_use) continue;
		if (!ec) return port;
	}
	if (cfg->allow_random_ports()) {
		// bind to port 0, i.e. let the operating system select a free port
		sock.bind(typename Protocol::endpoint(protocol, 0), ec);
		// query and return the port the socket was bound to
		if (!ec) return sock.local_endpoint().port();
	}
	throw std::runtime_error(
		"All local ports were found occupied. You may have more open outlets on this machine "
		"than your PortRange setting allows (see "
		"https://labstreaminglayer.readthedocs.io/info/network-connectivity.html"
		") or you have a problem with your network configuration.");
}

uint16_t lsl::bind_port_in_range(udp_socket &sock, asio::ip::udp protocol) {
	return bind_port_in_range_(sock, protocol);
}

uint16_t lsl::bind_and_listen_to_port_in_range(
	tcp_acceptor &acc, asio::ip::tcp protocol, int backlog) {
	uint16_t port = bind_port_in_range_(acc, protocol);
	acc.listen(backlog);
	return port;
}

namespace {
/// An int-valued IPPROTO_TCP socket option, for the keep-alive timing options Asio doesn't wrap
class tcp_int_option {
public:
	tcp_int_option(int name, int value) : name_(name), value_(value) {}
	template <typename Protocol> int level(const Protocol &) const { return IPPROTO_TCP; }
	template <typename Protocol> int name(const Protocol &) const { return name_; }
	template <typename Protocol> const int *data(const Protocol &) const { return &value_; }
	template <typename Protocol> std::size_t size(const Protocol &) const { return sizeof(value_); }

private:
	int name_, value_;
};

[[maybe_unused]] void set_tcp_int_option(tcp_socket &sock, int name, int value, const char *optname,
	int verbosity = loguru::Verbosity_WARNING) {
	asio::error_code ec;
	sock.set_option(tcp_int_option(name, value), ec);
	if (ec) VLOG_F(verbosity, "Could not set %s=%d: %s", optname, value, ec.message().c_str());
}
} // namespace

void lsl::set_tcp_keepalive(tcp_socket &sock) {
	const auto *cfg = api_config::get_instance();
	const int idle = cfg->tcp_keepalive_idle();
	if (idle <= 0) return;
	const int interval = cfg->tcp_keepalive_interval();
	[[maybe_unused]] const int count = cfg->tcp_keepalive_count();

	asio::error_code ec;
	sock.set_option(asio::socket_base::keep_alive(true), ec);
	if (ec) {
		LOG_F(WARNING, "Could not enable TCP keep-alive: %s", ec.message().c_str());
		return;
	}
#ifdef _WIN32
	// SIO_KEEPALIVE_VALS works on every Windows version; times are in milliseconds
	tcp_keepalive vals{1, static_cast<ULONG>(idle) * 1000, static_cast<ULONG>(interval) * 1000};
	DWORD bytes_returned = 0;
	if (WSAIoctl(sock.native_handle(), SIO_KEEPALIVE_VALS, &vals, sizeof(vals), nullptr, 0,
			&bytes_returned, nullptr, nullptr) == SOCKET_ERROR)
		LOG_F(WARNING, "Could not set the TCP keep-alive timing: error %d", WSAGetLastError());
#ifdef TCP_KEEPCNT
	// Older Windows versions don't have TCP_KEEPCNT and always send 10 probes
	set_tcp_int_option(sock, TCP_KEEPCNT, count, "TCP_KEEPCNT", 1);
#endif
#else
#if defined(TCP_KEEPIDLE)
	set_tcp_int_option(sock, TCP_KEEPIDLE, idle, "TCP_KEEPIDLE");
#elif defined(TCP_KEEPALIVE) // macOS
	set_tcp_int_option(sock, TCP_KEEPALIVE, idle, "TCP_KEEPALIVE");
#endif
#ifdef TCP_KEEPINTVL
	set_tcp_int_option(sock, TCP_KEEPINTVL, interval, "TCP_KEEPINTVL");
#endif
#ifdef TCP_KEEPCNT
	set_tcp_int_option(sock, TCP_KEEPCNT, count, "TCP_KEEPCNT");
#endif
#endif
}
