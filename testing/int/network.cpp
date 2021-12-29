#include "../src/cancellable_streambuf.h"
#include <asio/io_context.hpp>
#include <asio/ip/multicast.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/ip/v6_only.hpp>
#include <asio/read.hpp>
#include <asio/use_future.hpp>
#include <asio/write.hpp>
#include <catch2/catch.hpp>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

// clazy:excludeall=non-pod-global-static

using namespace asio;
using namespace std::chrono_literals;
using err_t = const asio::error_code &;

static uint16_t port = 28812;
static const char hello[] = "Hello World";
static const std::string hellostr(hello);

static std::mutex output_mutex;

asio::const_buffer hellobuf() { return asio::const_buffer(hello, sizeof(hello)); }


/// launches a task and waits for the underlying thread to have started
template <typename Fun> std::future<void> launch_task(Fun &&fun) {
	std::promise<void> started;
	auto started_fut = started.get_future();
	std::future<void> done_fut =
		std::async(std::launch::async, [&started, fn = std::forward<Fun>(fun)]() {
			started.set_value();
			fn();
		});
	started_fut.wait();
	return done_fut;
}

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

TEST_CASE("streambuf split reads", "[streambuf][network]") {
	asio::io_context io_ctx;
	lsl::cancellable_streambuf sb_read;
	ip::tcp::endpoint ep(ip::address_v4::loopback(), port++);
	ip::tcp::acceptor remote(io_ctx, ep, true);
	remote.listen(1);
	REQUIRE(sb_read.connect(ep) != nullptr);
	ip::tcp::socket sock(remote.accept());
	REQUIRE(sock.send(asio::buffer(hello, 3)) == 3);

	REQUIRE(sb_read.sbumpc() == hello[0]);
	auto done = launch_task([&]() {
		char buf[sizeof(hello)] = {0};
		auto bytes_read = sb_read.sgetn(buf, sizeof(hello) - 2);
		REQUIRE(bytes_read != std::streambuf::traits_type::eof());
		CHECK(bytes_read == sizeof(hello) - 2);
		REQUIRE(std::string(buf) == hellostr.substr(1));
	});
	sock.send(asio::buffer(hello + 3, 8));
	done.wait();

	std::vector<char> in_(65536 * 16), out_(65536 * 16);
	for (std::size_t i = 0; i < out_.size(); ++i) out_[i] = (i >> 8 ^ i) % 127;

	done = launch_task([&sb_read, &in_](){
		auto *dataptr = in_.data(), *endptr = dataptr + in_.size();
		while(dataptr != endptr) {
			std::streamsize bytes_read =
				sb_read.sgetn(dataptr, std::min<std::streamsize>(endptr - dataptr, 54));
			if(bytes_read == std::streambuf::traits_type::eof()) break;
			dataptr += bytes_read;
		}
	});
	for(const char*outptr = out_.data(), *endptr = outptr + out_.size(); outptr != endptr; outptr+=64)
		sock.send(asio::buffer(outptr, 64));
	done.wait();
	REQUIRE(std::equal(in_.begin(), in_.end(), out_.begin()));
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
				asio::error_code ec;
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

TEST_CASE("bindzero", "[network][basic]") {
	asio::io_context ctx;
	asio::ip::udp::socket sock(ctx, asio::ip::udp::v4());
	sock.bind(asio::ip::udp::endpoint(asio::ip::address_v4::any(), 0));
	REQUIRE(sock.local_endpoint().port() != 0);
}

#ifdef CATCH_CONFIG_ENABLE_BENCHMARKING

TEST_CASE("streambuf throughput", "[streambuf][network]") {
	asio::io_context io_ctx;
	asio::executor_work_guard<asio::io_context::executor_type> work(io_ctx.get_executor());
	auto background_io = launch_task([&]() { io_ctx.run(); });

	lsl::cancellable_streambuf sb_bench;
	ip::tcp::endpoint ep(ip::address_v4::loopback(), port++);
	ip::tcp::acceptor remote(io_ctx, ep, true);
	remote.listen();
	ip::tcp::socket sock(io_ctx);

	auto accept_fut = remote.async_accept(sock, asio::use_future);
	REQUIRE(sb_bench.connect(ep) != nullptr);
	REQUIRE(accept_fut.wait_for(2s) == std::future_status::ready);

	char buf_small[16] = "!Hello World!", buf_medium[256]{'\xab'}, buf_large[4096]{'\xab'};
	asio::mutable_buffer bufs[] = {
		asio::buffer(buf_small), asio::buffer(buf_medium), asio::buffer(buf_large)};

	std::vector<char> dummy_buffer;

	for (const auto &buf : bufs) {
		for (std::size_t chunksize : {1U, 16U, 256U}) {
			BENCHMARK_ADVANCED("Send;nchunk=" + std::to_string(chunksize) +
							   ";buf=" + std::to_string(buf.size()) +
							   ";n=" + std::to_string(chunksize * buf.size()))
			(Catch::Benchmark::Chronometer meter) {

				const auto total_bytes = buf.size() * chunksize * meter.runs();
				if (dummy_buffer.size() < total_bytes) dummy_buffer.resize(total_bytes);
				auto fut = asio::async_read(
					sock, asio::buffer(dummy_buffer.data(), total_bytes), asio::use_future);

				asio::steady_timer t(io_ctx, 5s);
				t.async_wait([&](err_t ec) { REQUIRE(ec == asio::error::operation_aborted); });
				meter.measure([&]() {
					for (auto chunk = 0U; chunk < chunksize; ++chunk) {
						auto res = sb_bench.sputn(reinterpret_cast<char *>(buf.data()), buf.size());
						REQUIRE(res != std::streambuf::traits_type::eof());
					}
					sb_bench.pubsync();
				});
				// Wait for the read operations to finish
				fut.wait();
				t.cancel();
			};
		}
	}
	for (const auto &buf : bufs) {
		for (int chunksize : {1, 16, 256}) {
			BENCHMARK_ADVANCED("Recv;nchunk=" + std::to_string(chunksize) +
							   ";buf=" + std::to_string(buf.size()) +
							   ";n=" + std::to_string(chunksize * buf.size()))
			(Catch::Benchmark::Chronometer meter) {
				const auto total_bytes = buf.size() * chunksize * meter.runs();

				if (dummy_buffer.size() < total_bytes) dummy_buffer.resize(total_bytes);
				asio::async_write(sock, asio::buffer(dummy_buffer.data(), total_bytes),
					[](err_t err, std::size_t /* unused */) { REQUIRE(!err); });
				std::this_thread::sleep_for(10ms);
				asio::steady_timer t(io_ctx, 5s);
				t.async_wait([&](err_t ec) { REQUIRE(ec == asio::error::operation_aborted); });
				meter.measure([&]() {
					for (int chunk = 0; chunk < chunksize; ++chunk) {
						auto res = sb_bench.sgetn(reinterpret_cast<char *>(buf.data()), buf.size());
						REQUIRE(res != std::streambuf::traits_type::eof());
					}
				});
				t.cancel();
			};
		}
	}
	asio::post(io_ctx, [&]() { io_ctx.stop(); });
	background_io.wait();
}
#endif
