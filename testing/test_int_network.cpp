#include "../src/cancellable_streambuf.h"
#include "catch.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

namespace asio = lslboost::asio;
using namespace asio;
using err_t = const lslboost::system::error_code &;

static uint16_t port = 28812;
static const char hello[] = "Hello World";
static const std::string hellostr(hello);

static std::mutex output_mutex;

asio::const_buffer hellobuf() { return asio::const_buffer(hello, sizeof(hello)); }

#define MINFO(str)                                                                                 \
	{                                                                                              \
		std::unique_lock<std::mutex> out_lock(output_mutex);                                       \
		UNSCOPED_INFO(str);                                                                        \
	}

// Check if a background operation (`task`) on a streambuf `sb` can be cancelled safely
template <typename T> void cancel_streambuf(T &&task, lsl::cancellable_streambuf &sb) {
	std::condition_variable cv;
	std::mutex mut;
	bool status{false};
	std::future<void> future = std::async(std::launch::async, [&]() {
		std::unique_lock<std::mutex> lock(mut);
		MINFO("Thread 1: started")
		status = true;
		lock.unlock();
		cv.notify_all();
		MINFO("Thread 1: starting socket operation")
		task();
		MINFO("Thread 1: socket operation finished")
	});
	// We need to wait until sb_blockconnect.connect() was called, but the
	// thread is blocked connecting so we can't let it signal it's ready
	// So we wait 200ms immediately after connect() is supposed to be called
	{
		std::unique_lock<std::mutex> lock(mut);
		cv.wait(lock, [&] { return status; });
	}

	if (future.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready)
		FAIL("Thread 1 finished too soon, couldn't test cancellation");
	MINFO("Thread 0: Closing socket…")
	sb.cancel();
	// Double cancel, shouldn't do anything dramatic
	sb.cancel();

	// Allow the thread 2 seconds to finish
	if (future.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
		FAIL("Cancellation timed out");
		sb.close();
	}
}

TEST_CASE("streambuf cancel connect()", "[streambuf][basic][network]") {
	asio::io_context io_ctx;
	lsl::cancellable_streambuf sb_connect;
	INFO("Thread 0: Binding remote socket and keeping it busy…")
	ip::tcp::endpoint ep(ip::address_v4::loopback(), port++);
	ip::tcp::acceptor remote(io_ctx, ip::tcp::v4());
	remote.bind(ep);
	// Create a socket that keeps connect()ing sockets hanging
	// On Windows, this requires an additional socket option, on Unix
	// a backlog size of 0 and a socket waiting for the connection to be accept()ed
	// On macOS, backlog 0 uses SOMAXCONN instead and 1 is correct
#ifdef _WIN32
	remote.set_option(asio::detail::socket_option::integer<SOL_SOCKET, SO_CONDITIONAL_ACCEPT>(1));
	remote.listen(0);
#else
#ifdef __APPLE__
	const int backlog = 1;
#else
	const int backlog = 0;
#endif
	remote.listen(backlog);
	lsl::cancellable_streambuf busykeeper;
	CHECK(busykeeper.connect(ep) != nullptr);
#endif
	INFO("Thread 0: Remote socket should be busy")

	cancel_streambuf(
		[&sb_connect, ep]() {
			sb_connect.connect(ep);
			MINFO(sb_connect.error().message())
			REQUIRE(sb_connect.sbumpc() == std::char_traits<char>::eof());
		},
		sb_connect);
}

TEST_CASE("unconnected streambufs don't crash", "[streambuf][basic][network]") {
	asio::io_context io_ctx;
	lsl::cancellable_streambuf sb_failedconnect;
	ip::tcp::endpoint ep(ip::address_v4::loopback(), 1);
	sb_failedconnect.connect(ep);
	sb_failedconnect.cancel();
	lsl::cancellable_streambuf().cancel();
}

TEST_CASE("cancel streambuf reads", "[streambuf][network][!mayfail]") {
	asio::io_context io_ctx;
	lsl::cancellable_streambuf sb_read;
	ip::tcp::endpoint ep(ip::address_v4::loopback(), port++);
	ip::tcp::acceptor remote(io_ctx, ep, true);
	remote.listen(1);
	INFO("Thread 0: Connecting…")
	sb_read.connect(ep);
	INFO("Thread 0: Connected (" << sb_read.error().message() << ')')
	ip::tcp::socket sock(remote.accept());
	sock.send(asio::buffer(hello, 1));
	REQUIRE(sb_read.sbumpc() == hello[0]);

	cancel_streambuf(
		[&sb_read]() {
			auto data = sb_read.sbumpc();
			MINFO(sb_read.error().message())
			CHECK(data == std::char_traits<char>::eof());
		},
		sb_read);
}

TEST_CASE("receive v4 packets on v6 socket", "[ipv6][network]") {
	const uint16_t test_port = port++;
	asio::io_context io_ctx;
	ip::udp::socket sock(io_ctx, ip::udp::v6());
	sock.set_option(ip::v6_only(false));
	sock.bind(ip::udp::endpoint(ip::address_v6::any(), test_port));

	ip::udp::socket sender_v4(io_ctx, ip::udp::v4()), sender_v6(io_ctx, ip::udp::v6());
	asio::const_buffer sbuf(hellobuf());
	char recvbuf[64] = {0};
	sender_v4.send_to(sbuf, ip::udp::endpoint(ip::address_v4::loopback(), test_port));
	auto recv_len = sock.receive(asio::buffer(recvbuf, sizeof(recvbuf) - 1));
	CHECK(recv_len == sizeof(hello));
	CHECK(hellostr == recvbuf);
	std::fill_n(recvbuf, recv_len, 0);

	sender_v6.send_to(sbuf, ip::udp::endpoint(ip::address_v6::loopback(), test_port));
	recv_len = sock.receive(asio::buffer(recvbuf, sizeof(recvbuf) - 1));
	CHECK(hellostr == recvbuf);
	std::fill_n(recvbuf, recv_len, 0);
}

TEST_CASE("ipaddresses", "[ipv6][network][basic]") {
	ip::address_v4 v4addr(ip::make_address_v4("192.168.172.1")),
		mcastv4(ip::make_address_v4("239.0.0.183"));
	ip::address_v6 v6addr = ip::make_address_v6(ip::v4_mapped_t(), v4addr);
	ip::address addr(v4addr), addr_mapped(v6addr);
	CHECK(!v4addr.is_multicast());
	CHECK(mcastv4.is_multicast());
	// mapped IPv4 multicast addresses aren't considered IPv6 multicast addresses
	CHECK(!ip::make_address_v6(ip::v4_mapped, mcastv4).is_multicast());
	CHECK(v6addr.is_v4_mapped());
	CHECK(addr != addr_mapped);
	CHECK(addr == ip::address(ip::make_address_v4(ip::v4_mapped, v6addr)));

	auto scoped = ip::make_address_v6("::1%3");
	CHECK(scoped.scope_id() == 3);
}

/// Can multiple sockets bind to the same port and receive all broad-/multicast packets?
TEST_CASE("reuseport", "[network][basic][!mayfail]") {
	// Linux: sudo ip link set lo multicast on; sudo ip mroute show table all

	auto addrstr = GENERATE((const char *)"224.0.0.1", "255.255.255.255", "ff03::1");
	SECTION(addrstr) {
		const uint16_t test_port = port++;
		INFO("Test port " + std::to_string(test_port))
		asio::io_context io_ctx(1);

		auto addr = ip::make_address(addrstr);
		if (!addr.is_multicast())
			REQUIRE((addr.is_v4() && addr.to_v4() == ip::address_v4::broadcast()));
		auto proto = addr.is_v4() ? ip::udp::v4() : ip::udp::v6();
		std::vector<ip::udp::socket> socks;
		for (int i = 0; i < 2; ++i) {
			socks.emplace_back(io_ctx, proto);
			auto &sock = socks.back();
			sock.set_option(ip::udp::socket::reuse_address(true));
			sock.bind(ip::udp::endpoint(proto, test_port));

			if (addr.is_multicast()) {
				lslboost::system::error_code ec;
				sock.set_option(ip::multicast::join_group(addr), ec);
				if (ec == error::no_such_device || ec == std::errc::address_not_available)
					FAIL("Couldn't join multicast group: " + ec.message());
			}
		}
		{
			ip::udp::socket outsock(io_ctx, proto);
			if (addr.is_multicast())
				outsock.set_option(ip::multicast::join_group(addr));
			else
				outsock.set_option(ip::udp::socket::broadcast(true));
			auto sent = outsock.send_to(hellobuf(), ip::udp::endpoint(addr, test_port));

			REQUIRE(sent == sizeof(hello));
			// outsock.close();
		}
		std::size_t received = 0;
		// set a timeout
		asio::steady_timer timeout(io_ctx, std::chrono::seconds(2));
		timeout.async_wait([&received](err_t err) {
			if (err == asio::error::operation_aborted) return;
			UNSCOPED_INFO(received);
			throw std::runtime_error("Test didn't finish in time");
		});
		char inbuf[sizeof(hello)] = {0};
		for (auto &insock : socks)
			insock.async_receive(asio::buffer(inbuf), [&](err_t err, std::size_t len) {
				CHECK(err.value() == 0);
				CHECK(len == sizeof(hello));
				CHECK(hellostr == inbuf);
				if (++received == socks.size()) timeout.cancel();
			});
		io_ctx.run();
	}
}
