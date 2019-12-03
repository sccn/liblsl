#include "../src/cancellable_streambuf.h"
#include "catch.hpp"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

namespace asio = lslboost::asio;
namespace ip = lslboost::asio::ip;
using lslboost::system::error_code;
typedef lsl::cancellable_streambuf cancellable_streambuf;

const uint16_t port = 28812;

template <typename T> void test_cancel_thread(T &&task, cancellable_streambuf &sb) {
	std::condition_variable cv;
	std::mutex mut;
	bool status = false;
	auto future = std::async(std::launch::async, [&]() {
		std::unique_lock<std::mutex> lock(mut);
		INFO("Thread 1: started");
		status = true;
		lock.unlock();
		cv.notify_all();
		INFO("Thread 1: starting socket operation");
		task();
		INFO("Thread 1: socket operation finished");
	});
	// We need to wait until sb_blockconnect.connect() was called, but the
	// thread is blocked connecting so we can't let it signal it's ready
	// So we wait 200ms immediately after connect() is supposed to be called
	{
		std::unique_lock<std::mutex> lock(mut);
		cv.wait(lock, [&] { return status; });
	}

	if (future.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready)
		INFO("Thread 1 finished too soon, couldn't test cancellation");
	INFO("Thread 0: Closing socket…");
	sb.cancel();
	// Double cancel, shouldn't do anything dramatic
	sb.cancel();

	// Allow the thread 2 seconds to finish
	if (future.wait_for(std::chrono::seconds(2)) != std::future_status::ready)
		throw std::runtime_error("Thread 0: Thread didn't join!");
	else {
		INFO("Thread 0: Thread was successfully canceled");
		future.get();
	}
}

TEST_CASE("streambufs can connect", "[streambuf][basic]") {
	asio::io_context io_ctx;
	cancellable_streambuf sb_connect;
	INFO("Thread 0: Binding remote socket and keeping it busy…");
	ip::tcp::endpoint ep(ip::address_v4::loopback(), port + 1);
	ip::tcp::acceptor remote(io_ctx);
	remote.open(ip::tcp::v4());
	remote.bind(ep);
	// Create a socket that keeps connect()ing sockets hanging
	// On Windows, this requires an additional socket options, on Unix
	// a backlog size of 0 and a socket waiting for the connection to be accept()ed
	// On macOS, backlog 0 uses SOMAXCONN instead and 1 is correct
#ifdef _WIN32
	remote.set_option(asio::detail::socket_option::integer<SOL_SOCKET, SO_CONDITIONAL_ACCEPT>(1));
	remote.listen(0);
#else
#ifdef __APPLE__
	int backlog = 1;
#else
	int backlog = 0;
#endif
	remote.listen(backlog);
	cancellable_streambuf busykeeper;
	busykeeper.connect(ep);
#endif
	INFO("Thread 0: Remote socket should be busy");

	test_cancel_thread([&sb_connect, ep]() { sb_connect.connect(ep); }, sb_connect);
	remote.close();
}

TEST_CASE("streambufs can transfer data", "[streambuf][read]") {
	asio::io_context io_ctx;
	cancellable_streambuf sb_read;
	ip::tcp::endpoint ep(ip::address_v4::loopback(), port + 1);
	ip::tcp::acceptor remote(io_ctx, ep, true);
	remote.listen(1);
	INFO("Thread 0: Connecting…");
	sb_read.connect(ep);
	INFO("Thread 0: Connected (" << sb_read.puberror().message() << ')');
	ip::tcp::socket sock(io_ctx);
	remote.accept(sock);

	test_cancel_thread(
		[&sb_read]() {
			int c = sb_read.sgetc();
			INFO("Thread 1: Read char " << c);
		},
		sb_read);
}
