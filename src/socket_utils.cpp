#include "socket_utils.h"
#include "api_config.h"
#include "common.h"
#include <boost/endian/conversion.hpp>

using namespace lsl;

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
	const api_config *cfg = api_config::get_instance();
	lslboost::system::error_code ec;
	for (uint16_t port = cfg->base_port(), e = port + cfg->port_range(); port < e; port++) {
		sock.bind(typename Protocol::endpoint(protocol, port), ec);
		if (ec == lslboost::system::errc::address_in_use) continue;
		if (!ec) return port;
	}
	if (cfg->allow_random_ports()) {
		for (int k = 0; k < 100; ++k) {
			uint16_t port = 1025 + rand() % 64000;
			sock.bind(typename Protocol::endpoint(protocol, port), ec);
			if (ec == lslboost::system::errc::address_in_use) continue;
			if (!ec) return port;
		}
	}
	return 0;
}

const std::string all_ports_bound_msg(
	"All local ports were found occupied. You may have more open outlets on this machine than your "
	"PortRange setting allows (see "
	"https://labstreaminglayer.readthedocs.io/info/network-connectivity.html"
	") or you have a problem with your network configuration.");

uint16_t lsl::bind_port_in_range(udp::socket &acc, udp protocol) {
	uint16_t port = bind_port_in_range_(acc, protocol);
	if (!port) throw std::runtime_error(all_ports_bound_msg);
	return port;
}

uint16_t lsl::bind_and_listen_to_port_in_range(tcp::acceptor &sock, tcp protocol, int backlog) {
	uint16_t port = bind_port_in_range_(sock, protocol);
	if (!port) throw std::runtime_error(all_ports_bound_msg);
	sock.listen(backlog);
	return port;
}
