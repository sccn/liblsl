#pragma once
#include <asio/ip/address.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace lsl {

class netif {
public:
	asio::ip::address addr;
	uint32_t ifindex;
	std::string name;
};

/// Enumerate all local interface addresses (IPv6 index, IPv4 address)-pairs
std::vector<netif> get_local_interfaces();
} // namespace lsl
