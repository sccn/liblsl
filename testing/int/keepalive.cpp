#include "api_config.h"
#include "socket_utils.h"
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <catch2/catch_test_macros.hpp>
#ifndef _WIN32
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

// clazy:excludeall=non-pod-global-static

#ifndef _WIN32
static int get_tcp_option(tcp_socket &sock, int name) {
	int value = -1;
	socklen_t len = sizeof(value);
	REQUIRE(getsockopt(sock.native_handle(), IPPROTO_TCP, name, &value, &len) == 0);
	return value;
}
#endif

TEST_CASE("set_tcp_keepalive turns on keep-alive with the configured timing", "[network][basic]") {
	const auto *cfg = lsl::api_config::get_instance();
	REQUIRE(cfg->tcp_keepalive_idle() > 0);

	asio::io_context ctx;
	tcp_acceptor acceptor(ctx, asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0));
	tcp_socket client(ctx), server(ctx);
	client.connect(acceptor.local_endpoint());
	acceptor.accept(server);

	asio::socket_base::keep_alive keep_alive;
	client.get_option(keep_alive);
	CHECK(!keep_alive.value());

	lsl::set_tcp_keepalive(client);
	client.get_option(keep_alive);
	CHECK(keep_alive.value());

#ifndef _WIN32
#if defined(TCP_KEEPIDLE)
	CHECK(get_tcp_option(client, TCP_KEEPIDLE) == cfg->tcp_keepalive_idle());
#elif defined(TCP_KEEPALIVE)
	CHECK(get_tcp_option(client, TCP_KEEPALIVE) == cfg->tcp_keepalive_idle());
#endif
#ifdef TCP_KEEPINTVL
	CHECK(get_tcp_option(client, TCP_KEEPINTVL) == cfg->tcp_keepalive_interval());
#endif
#ifdef TCP_KEEPCNT
	CHECK(get_tcp_option(client, TCP_KEEPCNT) == cfg->tcp_keepalive_count());
#endif
#endif
}
