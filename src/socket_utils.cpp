#include "socket_utils.h"
#include "api_config.h"
#include "common.h"
#include <boost/endian/conversion.hpp>

double lsl::measure_endian_performance() {
	const double measure_duration = 0.01;
	const double t_end = lsl_clock() + measure_duration;
	uint64_t data = 0x01020304;
	double k;
	for (k = 0; ((int)k & 0xFF) != 0 || lsl_clock() < t_end; k++)
		lslboost::endian::endian_reverse_inplace(data);
	return k;
}

template <typename Socket, typename Protocol>
uint16_t bind_port_in_range_(Socket &sock, Protocol protocol) {
	const auto *cfg = lsl::api_config::get_instance();
	lslboost::system::error_code ec;
	for (uint16_t port = cfg->base_port(), e = port + cfg->port_range(); port < e; port++) {
		sock.bind(typename Protocol::endpoint(protocol, port), ec);
		if (ec == lslboost::system::errc::address_in_use) continue;
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

uint16_t lsl::bind_port_in_range(asio::ip::udp::socket &sock, asio::ip::udp protocol) {
	return bind_port_in_range_(sock, protocol);
}

uint16_t lsl::bind_and_listen_to_port_in_range(
	asio::ip::tcp::acceptor &acc, asio::ip::tcp protocol, int backlog) {
	uint16_t port = bind_port_in_range_(acc, protocol);
	acc.listen(backlog);
	return port;
}
