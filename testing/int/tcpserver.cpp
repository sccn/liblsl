#include "../common/bytecmp.hpp"
#include "sample.h"
#include "send_buffer.h"
#include "stream_info_impl.h"
#include "tcp_server.h"
#include <asio/read.hpp>
#include <asio/read_until.hpp>
#include <asio/write.hpp>
#include <catch2/catch.hpp>
#include <functional>
#include <sstream>
#include <thread>

// clazy:excludeall=non-pod-global-static

using namespace asio::ip;
using err_t = const asio::error_code &;
using sock_t = tcp::socket;
using sock_p = std::shared_ptr<sock_t>;

/// RAII wrapper that takes care of shutting down the IO thread when an exception happens
class tcp_server_wrapper {
	std::shared_ptr<lsl::tcp_server> srv;
	std::shared_ptr<asio::io_context> srv_ctx;
	std::unique_ptr<std::thread> thread;
public:
	tcp_server_wrapper(std::shared_ptr<lsl::stream_info_impl> info) {
		auto sendbuf = std::make_shared<lsl::send_buffer>(10);
		srv_ctx = std::make_shared<asio::io_context>(1);
		auto factory =
			std::make_shared<lsl::factory>(info->channel_format(), info->channel_count(), 10);
		srv = std::make_shared<lsl::tcp_server>(info, srv_ctx, sendbuf, factory, 5, true, true);
		srv->begin_serving();
	}
	~tcp_server_wrapper() noexcept {
		srv->end_serving();
		if (thread)
			thread->join();
		else
			try {
				srv_ctx->run();
			} catch (std::exception &e) { INFO(e.what()); }
	}
	void run() {
		thread = std::make_unique<std::thread>([this]() {
			try {
				this->srv_ctx->run();
			} catch (std::exception &e) { INFO(e.what()); }
		});
	}
	lsl::tcp_server* operator->() { return srv.get(); }

};

// testpattern timestamp 123456.789
#define TESTPAT_TIMESTAMP "\xc9v\xbe\x9f\f$\xfe@"
// testpattern for two strings
#define TESTPAT_STR "\1\00210\1\3-11"

void send_request(asio::io_context &ctx, const tcp::endpoint &ep, asio::const_buffer request,
	std::function<void(sock_p)> write_cb) {
	auto sock = std::make_shared<sock_t>(ctx);
	sock->async_connect(ep, [=](err_t connect_err) {
		INFO(connect_err.message())
		REQUIRE(connect_err.value() == 0);
		asio::async_write(*sock, request,
			[=, expected_bytes = request.size()](err_t write_err, std::size_t sent_bytes) {
				INFO("Sent " << sent_bytes << " bytes, outcome: " << write_err.message())
				REQUIRE(write_err.value() == 0);
				REQUIRE(sent_bytes == expected_bytes);
				write_cb(sock);
			});
	});
}

auto with_read_callback(const char *name, std::function<void(const std::string &)> fun) {
	return [name, fun = std::move(fun)](sock_p sock) {
		auto buf = std::make_shared<std::string>();
		asio::async_read(*sock, asio::dynamic_buffer(*buf),
			[sock, buf, name, fun
#if !defined(_MSC_VER) || _MSC_VER > 1930
						 = std::move(fun)
#endif
						 ](err_t read_err, std::size_t len) {
				INFO("Test " << name << "\t– read " << len
							 << " bytes, outcome: " << read_err.message())
				if (read_err) REQUIRE(read_err == asio::error::eof);
				fun(*buf);
			});
	};
}

void check_streamfeed_100_response(const std::string &res) {
	REQUIRE(res.substr(0, 4) == "\x7f\x01\x09\2");
	INFO(bytes_to_hexstr(res.substr(0, 10)));
	auto info_len = *reinterpret_cast<const uint16_t*>(res.data() + 4);

	REQUIRE(static_cast<int>(res.size()) > 6 + info_len);

	auto info = lsl::stream_info_impl();
	info.from_fullinfo_message(res.substr(6, info_len));
	REQUIRE(info.source_id() == "abc123");

	// precomputed string pattern for 2 string channels, serialized with Boost.Archive
	const char pat_str[] = "\0\0"
						   "\1\2\b" TESTPAT_TIMESTAMP TESTPAT_STR  // first sample
						   "\1\2\b" TESTPAT_TIMESTAMP TESTPAT_STR; // second, identical sample
	const char pat_f32[] = "\0\0\1\2\b" TESTPAT_TIMESTAMP		   // first sample
						   "\4\0\0\x80@"						   // 4 bytes, raw float data
						   "\4\0\0\xa0\xc0"
						   "\4\0\0\xc0@"
						   "\1\2\b" TESTPAT_TIMESTAMP // second sample
						   "\4\0\0\0@"
						   "\4\0\0@\xc0"
						   "\4\0\0\x80@";
	const std::string pat = info.channel_format() == cft_string
								? std::string(pat_str, sizeof(pat_str) - 1)
								: std::string(pat_f32, sizeof(pat_f32) - 1);
	cmp_binstr(res.substr(6 + info_len), pat);
}

TEST_CASE("tcpserver", "[network]") {

	asio::io_context ctx(1);

	auto info = std::make_shared<lsl::stream_info_impl>("TCP_str", "", 2, 4., cft_string, "abc123");
	tcp_server_wrapper tcp_server(info);
	tcp::endpoint ep(address_v4(0x7f000001), info->v4data_port());

	send_request(ctx, ep, asio::buffer("LSL:streamfeed/110 \n\r\n\r\n"),
		with_read_callback("basic", [](const std::string &res) {
			REQUIRE(res.substr(0, 14) == "LSL/110 200 OK");
			auto endofheader = res.find("\r\n\r\n");
			REQUIRE(endofheader != std::string::npos);
			std::string received_pattern = res.substr(endofheader + 4);
			std::string expected = "\2" TESTPAT_TIMESTAMP TESTPAT_STR;
			expected += expected;
			cmp_binstr(expected, received_pattern);
		}));

	send_request(ctx, ep, asio::buffer("LSL:streamfeed/199 \nNative-byte-order:4321\r\n\r\n"),
		with_read_callback("endian", [](const std::string &res) {
			REQUIRE(res.substr(0, 14) == "LSL/110 200 OK");
			REQUIRE(res.find("Byte-Order: 4321") != std::string::npos);
		}));

	send_request(ctx, ep, asio::buffer("LSL:streamfeed\n0 0\r\n"),
		with_read_callback("streamfeed 100", check_streamfeed_100_response));

	send_request(ctx, ep, asio::buffer("LSL:fullinfo\r\n"),
		with_read_callback("fullinfo", [expected = info->to_fullinfo_message()](
										   const std::string &res) { REQUIRE(res == expected); }));

	tcp_server.run();
	ctx.run();
}

TEST_CASE("tcpserver_float", "[network]") {
	auto srv_ctx = std::make_shared<asio::io_context>(1);
	asio::io_context ctx(1);

	auto info =
		std::make_shared<lsl::stream_info_impl>("TCP_f32", "", 3, 4., cft_float32, "abc123");
	tcp_server_wrapper tcp_server(info);
	tcp::endpoint ep(address_v4(0x7f000001), info->v4data_port());

	send_request(ctx, ep, asio::buffer("LSL:streamfeed/199 \nsupports-subnormals: 0\r\n\r\n"),
		with_read_callback("suppress-subnormals", [](const std::string &res) {
			REQUIRE(res.substr(0, 14) == "LSL/110 200 OK");
			REQUIRE(res.find("Suppress-Subnormals: 1") != std::string::npos);
			auto endofheader = res.find("\r\n\r\n");
			REQUIRE(endofheader != std::string::npos);
			std::string received_pattern = res.substr(endofheader + 4);
			const char expected[] = "\2" TESTPAT_TIMESTAMP // sample 1
									"\0\0\x80@"
									"\0\0\xa0\xc0"
									"\0\0\xc0@"
									"\2" TESTPAT_TIMESTAMP // sample 2
									"\0\0\0@"
									"\0\0@\xc0"
									"\0\0\x80@";
			cmp_binstr(std::string(expected, sizeof(expected) - 1), received_pattern);
		}));

	send_request(ctx, ep, asio::buffer("LSL:streamfeed/110 \nNative-byte-order:4321\r\n\r\n"),
		with_read_callback("endian", [](const std::string &res) {
			REQUIRE(res.substr(0, 14) == "LSL/110 200 OK");
			REQUIRE(res.find("Byte-Order: 4321") != std::string::npos);
			auto endofheader = res.find("\r\n\r\n");
			REQUIRE(endofheader != std::string::npos);
			std::string received_pattern = res.substr(endofheader + 4);

			const char expected[] = "\2" "@\xfe$\f\x9f\xbev\xc9" // sample 1
									"@\x80\0\0"
									"\xc0\xa0\0\0"
									"@\xc0\0\0"
									"\2" "@\xfe$\f\x9f\xbev\xc9" // sample 2
									"@\0\0\0"
									"\xc0@\0\0"
									"@\x80\0\0";
			cmp_binstr(std::string(expected, sizeof(expected) - 1), received_pattern);
		}));

	send_request(ctx, ep, asio::buffer("LSL:streamfeed\n0 0\r\n"),
		with_read_callback("streamfeed 100", check_streamfeed_100_response));

	tcp_server.run();
	ctx.run();
}
