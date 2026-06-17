#include "api_config.h"
#include <catch2/catch_test_macros.hpp>
#include <lsl_cpp.h>
#include <string>
#include <vector>

// Regression test for the same-machine "unicast lottery" (only one local stream resolves).
//
// With ResolveScope=machine the only discovery target is the machine address (127.0.0.1) at the
// shared multicast_port. That port is unicast, not multicast, so a query datagram is delivered to
// exactly one of the responder sockets bound there — only one outlet answers, and which one
// depends on bind order. Several outlets are therefore created, but pre-fix only one is found.
//
// The fix additionally probes the machine addresses across the per-stream service-port range,
// where every outlet owns a unique socket, so all of them are discoverable.
//
// Sets the api_config singleton, so (like runtime_config) it lives in its own executable.

TEST_CASE("all same-machine streams resolve under ResolveScope=machine", "[resolver][machine]") {
	lsl::api_config::set_api_config_content(
		"[ports]\n"
		"IPv6 = disable\n"
		"[multicast]\n"
		"ResolveScope = machine\n"
		"MachineAddresses = {127.0.0.1}\n"
		"[lab]\n"
		"SessionID = resolve_machine_test\n"
		"KnownPeers = {}\n"); // no KnownPeers: discovery relies solely on the machine address

	// Stand up several outlets on this machine. Each binds its own per-stream service port.
	constexpr int n = 3;
	std::vector<lsl::stream_outlet> outlets;
	outlets.reserve(n);
	for (int i = 0; i < n; i++) {
		lsl::stream_info info("machtest" + std::to_string(i), "machtest", 1, lsl::IRREGULAR_RATE,
			lsl::cf_float32, "machsrc" + std::to_string(i));
		outlets.emplace_back(info);
	}

	// Resolve by the shared type. Pre-fix the unicast lottery yields fewer than n; the fix probes
	// each outlet's unique service port, so all n are found.
	std::vector<lsl::stream_info> found = lsl::resolve_stream("type", "machtest", n, 5.0);

	CHECK(found.size() == n);
}
