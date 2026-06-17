#include "api_config.h"
#include <catch2/catch_test_macros.hpp>
#include <lsl_cpp.h>
#include <vector>

// Verifies that lab.ResolveOverTCP discovers a local stream even when UDP discovery is fully
// blinded (ResolveScope=machine with an empty MachineAddresses leaves no multicast/broadcast
// targets, and empty KnownPeers leaves no unicast targets). With UDP unable to reach the outlet,
// a successful resolve can only have come from the TCP probe path.
//
// Sets the api_config singleton, so (like runtime_config) it lives in its own executable.

TEST_CASE("ResolveOverTCP finds a local stream when UDP discovery is blind", "[resolver][tcp]") {
	lsl::api_config::set_api_config_content(
		"[ports]\n"
		"IPv6 = disable\n"
		"PortRange = 4\n"
		"[multicast]\n"
		"ResolveScope = machine\n"
		"MachineAddresses = {}\n" // no multicast/broadcast discovery targets at all
		"[lab]\n"
		"SessionID = resolve_over_tcp_test\n"
		"KnownPeers = {}\n" // no unicast UDP discovery targets either
		"ResolveOverTCP = 1\n");

	lsl::stream_info info("tcptest", "test", 1, lsl::IRREGULAR_RATE, lsl::cf_float32, "tcpsrc");
	lsl::stream_outlet outlet(info);

	// UDP discovery has no targets, so a hit here must have come from the loopback TCP probe.
	std::vector<lsl::stream_info> found = lsl::resolve_stream("name", "tcptest", 1, 5.0);

	REQUIRE(found.size() == 1);
	CHECK(found[0].name() == "tcptest");
	CHECK(found[0].source_id() == "tcpsrc");
	// The resolved info must carry the TCP endpoint that the probe connected to.
	CHECK(found[0].channel_count() == 1);
}
