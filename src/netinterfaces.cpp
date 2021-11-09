#include "netinterfaces.h"
#include <cstring>
#include <loguru.hpp>

asio::ip::address_v6 sinaddr_to_asio(sockaddr_in6 *addr) {
	asio::ip::address_v6::bytes_type buf;
	memcpy(buf.data(), addr->sin6_addr.s6_addr, sizeof(addr->sin6_addr));
	return asio::ip::make_address_v6(buf, addr->sin6_scope_id);
}

#if defined(_WIN32)
#undef UNICODE
#include <winsock2.h>
// Headers that need to be included after winsock2.h:
#include <iphlpapi.h>
#include <ws2ipdef.h>

typedef IP_ADAPTER_UNICAST_ADDRESS_LH Addr;
typedef IP_ADAPTER_ADDRESSES *AddrList;

std::vector<lsl::netif> lsl::get_local_interfaces() {
	// It's a windows machine, we assume it has 512KB free memory
	DWORD outBufLen = 1 << 19;
	AddrList ifaddrs = (AddrList) new char[outBufLen];

	std::vector<lsl::netif> ret;

	ULONG res = GetAdaptersAddresses(AF_UNSPEC,
		GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, ifaddrs,
		&outBufLen);

	if (res == NO_ERROR) {
		for (AddrList addr = ifaddrs; addr != 0; addr = addr->Next) {
			// Interface isn't up or doesn't support multicast? Skip it.
			LOG_F(INFO, "netif '%s' (status: %d, multicast: %d", addr->AdapterName,
				addr->OperStatus, !addr->NoMulticast);
			if (addr->OperStatus != IfOperStatusUp) continue;
			if (addr->NoMulticast) continue;

			lsl::netif if_;
			if_.name = addr->AdapterName;
			if_.ifindex = addr->IfIndex;

			// Find the first IPv4 address
			if (addr->Ipv4Enabled) {
				for (Addr *uaddr = addr->FirstUnicastAddress; uaddr != 0; uaddr = uaddr->Next) {
					if (uaddr->Address.lpSockaddr->sa_family != AF_INET) continue;

					if_.addr = asio::ip::make_address_v4(
						ntohl(reinterpret_cast<sockaddr_in *>(uaddr->Address.lpSockaddr)
								  ->sin_addr.s_addr));
					ret.emplace_back(std::move(if_));
					break;
				}
			}

			if (addr->Ipv6Enabled) {
				LOG_F(INFO, "\tIPv6 ifindex %d", if_.ifindex);
				for (Addr *uaddr = addr->FirstUnicastAddress; uaddr != 0; uaddr = uaddr->Next) {
					if (uaddr->Address.lpSockaddr->sa_family != AF_INET6) continue;

					if_.addr = sinaddr_to_asio(
						reinterpret_cast<sockaddr_in6 *>(uaddr->Address.lpSockaddr));
					ret.emplace_back(std::move(if_));
					break;
				}
			}
		}
	} else {
		LOG_F(ERROR, "Couldn't enumerate network interfaces: %d", res);
	}
	delete[]((char *)ifaddrs);
	return ret;
}
#elif defined(__APPLE__) || defined(__linux__) && (!defined(__ANDROID_API__) || __ANDROID_API__ >= 24)
#include <ifaddrs.h>
#include <net/if.h>

std::vector<lsl::netif> lsl::get_local_interfaces() {
	std::vector<lsl::netif> res;
	ifaddrs *ifs;
	if (getifaddrs(&ifs)) {
		LOG_F(ERROR, "Couldn't enumerate network interfaces: %d", errno);
		return res;
	}
	for (auto *addr = ifs; addr != nullptr; addr = addr->ifa_next) {
		// No address? Skip.
		if (addr->ifa_addr == nullptr) continue;
		LOG_F(INFO, "netif '%s' (status: %d, multicast: %d, broadcast: %d)", addr->ifa_name,
			addr->ifa_flags & IFF_MULTICAST, addr->ifa_flags & IFF_UP,
			addr->ifa_flags & IFF_BROADCAST);
		// Interface doesn't support multicast? Skip.
		if (!(addr->ifa_flags & IFF_MULTICAST)) continue;
		// Interface isn't active? Skip.
		if (!(addr->ifa_flags & IFF_UP)) continue;

		lsl::netif if_;

		if (addr->ifa_addr->sa_family == AF_INET) {
			if_.addr = asio::ip::make_address_v4(
				ntohl(reinterpret_cast<sockaddr_in *>(addr->ifa_addr)->sin_addr.s_addr));
			LOG_F(INFO, "\tIPv4 addr: %x", if_.addr.to_v4().to_uint());
		} else if (addr->ifa_addr->sa_family == AF_INET6) {
			if_.addr = sinaddr_to_asio(reinterpret_cast<sockaddr_in6 *>(addr->ifa_addr));
			LOG_F(INFO, "\tIPv6 addr: %s", if_.addr.to_string().c_str());
		} else
			continue;

		if_.ifindex = if_nametoindex(addr->ifa_name);
		res.emplace_back(std::move(if_));
	}
	freeifaddrs(ifs);
	return res;
}
#else

std::vector<lsl::netif> get_interface_addresses() {
	LOG_F(WARNING, "No implementation to enumerate network interfaces found.");
	return std::vector<lsl::netif>();
}
#endif
