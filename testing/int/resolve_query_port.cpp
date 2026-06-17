#include "api_config.h"
#include "resolver_impl.h"
#include <asio/buffer.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/udp.hpp>
#include <asio/steady_timer.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <thread>

using asio::ip::make_address;
using asio::ip::udp;

// Regression test for the resolver "firewall asymmetry" (NETWORK_FAILURE_MODES.md 1a).
//
// A resolve query advertises a return port in its payload but, before the fix, is
// transmitted from a *different* socket (unicast_socket_) whose ephemeral source port
// does not match. A stateful firewall therefore sees the outlet's reply (sent to the
// advertised return port) as unsolicited inbound and drops it. The fix sends the query
// *from* recv_socket_ so the datagram source port equals the advertised return port,
// and the firewall sees a normal request/reply flow.
//
// We verify this deterministically, with NO firewall, by standing up a fake responder
// on loopback :16571, capturing the query, and comparing the datagram source port to
// the return port embedded in the payload.
//
// Sets the api_config singleton, so (like runtime_config) this lives in its own
// executable to start from a fresh, uninitialized config.

TEST_CASE("resolve query is sent from the advertised return port", "[resolver][network]") {
	// Point discovery at loopback so the query lands on our fake responder, and pin to
	// IPv4 so exactly one query stream is produced.
	lsl::api_config::set_api_config_content(
		"[ports]\n"
		"IPv6 = disable\n"
		"[multicast]\n"
		"AddressesOverride = {127.0.0.1}\n"
		"[lab]\n"
		"SessionID = resolve_query_port_test\n");

	const uint16_t multicast_port = lsl::api_config::get_instance()->multicast_port();

	// --- fake responder bound to loopback :16571 ---
	asio::io_context resp_io(1);
	udp::socket responder(resp_io);
	responder.open(udp::v4());
	responder.set_option(udp::socket::reuse_address(true));
	// Bind specifically to 127.0.0.1 so a more-specific match wins over any wildcard-bound
	// LSL outlet that might already be running on this machine.
	responder.bind(udp::endpoint(make_address("127.0.0.1"), multicast_port));

	udp::endpoint sender;
	char buf[2048] = {0};
	bool received = false;
	uint16_t datagram_source_port = 0;
	uint16_t advertised_return_port = 0;

	// Deadline first, so the receive handler can cancel it (declared before the handler
	// that references it).
	asio::steady_timer deadline(resp_io, std::chrono::seconds(3));
	deadline.async_wait([&](const asio::error_code &) { responder.cancel(); });

	responder.async_receive_from(asio::buffer(buf), sender,
		[&](const asio::error_code &ec, std::size_t len) {
			if (ec) return;
			received = true;
			datagram_source_port = sender.port();
			// Payload (resolve_attempt_udp):
			//   LSL:shortinfo\r\n
			//   <query>\r\n
			//   <return_port> <query_id>\r\n
			std::istringstream req(std::string(buf, buf + len));
			std::string method, query;
			std::getline(req, method);
			std::getline(req, query);
			req >> advertised_return_port;
			deadline.cancel(); // let resp_io.run() return promptly
		});

	// Drive a resolve in the background: it sends a query wave to 127.0.0.1:16571 and then
	// times out (no real outlet to find).
	std::thread resolver_thread([] {
		lsl::resolver_impl resolver;
		resolver.resolve_oneshot("session_id='resolve_query_port_test'", 1, 1.0);
	});

	resp_io.run(); // returns once a query is captured (or the deadline fires)
	resolver_thread.join();

	REQUIRE(received); // a query datagram reached our loopback responder
	// Pre-fix (1a): source port != return port  -> this CHECK fails (pins the bug).
	// Post-fix:     query sent from recv_socket_ -> they match.
	CHECK(advertised_return_port == datagram_source_port);
}
