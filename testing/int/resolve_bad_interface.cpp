#include "api_config.h"
#include <catch2/catch_test_macros.hpp>
#include <lsl_cpp.h>
#include <vector>

// Regression test for a bad multicast interface aborting / terminating stream resolution.
//
// The resolver selects the outbound interface for each multicast send via set_option(
// outbound_interface(...)). An interface address that cannot be used for IP_MULTICAST_IF (a stale
// or removed adapter, a VPN utun, AWDL, a Hyper-V/VirtualBox adapter, or simply an address that is
// not local) makes that call fail. Pre-fix it used the throwing overload, and because the call runs
// inside an asio completion handler the exception propagated out of io_->run() — aborting the whole
// resolve wave (oneshot) or calling std::terminate() from the continuous resolver's background
// thread. Here 203.0.113.1 is a TEST-NET-3 address (RFC 5737) that is never assignable locally.
//
// The fix uses the error_code overload and carries on, so the stream is still resolved and the
// process does not crash.
//
// Sets the api_config singleton, so (like runtime_config) it lives in its own executable.

TEST_CASE("a bad multicast interface does not abort resolution", "[resolver][interfaces]") {
	lsl::api_config::set_api_config_content(
		"[ports]\n"
		"IPv6 = disable\n"
		"[multicast]\n"
		"ResolveScope = machine\n"
		"MachineAddresses = {127.0.0.1}\n"
		"Interfaces = {127.0.0.1, 203.0.113.1}\n" // 203.0.113.1 is unusable for IP_MULTICAST_IF
		"[lab]\n"
		"SessionID = resolve_bad_interface_test\n"
		"KnownPeers = {}\n");

	lsl::stream_info info("ifacetest", "test", 1, lsl::IRREGULAR_RATE, lsl::cf_float32, "ifacesrc");
	lsl::stream_outlet outlet(info);

	// Pre-fix this either finds nothing (wave aborted) or crashes; post-fix the bad interface is
	// skipped and the stream is resolved normally.
	std::vector<lsl::stream_info> found = lsl::resolve_stream("name", "ifacetest", 1, 5.0);

	REQUIRE(found.size() == 1);
	CHECK(found[0].name() == "ifacetest");
}
