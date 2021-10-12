#include "stream_info_impl.h"
#include "udp_server.h"
#include <catch2/catch.hpp>
#include <sstream>
#include <thread>

using namespace lslboost::asio::ip;

TEST_CASE("timesync", "[basic][latency]") {
	auto info =
		std::make_shared<lsl::stream_info_impl>("Dummy", "dummy", 1, 1., cft_int8, "abcdef123");
	lslboost::asio::io_context ctx;
	auto udp_server = std::make_shared<lsl::udp_server>(info, ctx, lslboost::asio::ip::udp::v4());
	udp::endpoint ep(address_v4(0x7f000001), info->v4service_port());

	INFO(info->to_shortinfo_message())
	udp_server->begin_serving();
	std::thread iothread([&ctx]() { ctx.run(); });

	const char request[] = "LSL:timedata\n1 0.0";
	asio::basic_datagram_socket<asio::ip::udp, asio::io_context::executor_type> sock(ctx, udp::endpoint());
	char buf[500]={0};
	BENCHMARK("timedata") {
		sock.send_to(lslboost::asio::const_buffer(request, sizeof(request)-1), ep);
		REQUIRE(sock.receive(lslboost::asio::mutable_buffer(buf, 500)) > 0);
	};
	udp_server->end_serving();
	ctx.stop();
	iothread.join();
}
