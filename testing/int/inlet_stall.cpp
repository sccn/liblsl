#include "api_config.h"
#include "common.h"
#include "stream_info_impl.h"
#include "stream_inlet_impl.h"
#include "stream_outlet_impl.h"
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// Regression tests for inlets whose data connection goes silently dead (half-open), see
// https://github.com/sccn/liblsl/issues/326
//
// A half-open connection is one where the network path stops delivering packets without either
// side receiving a FIN or RST, e.g. a pulled cable, a Wi-Fi drop or an expired NAT entry. The
// inlet's blocking read then never returns, and stream recovery only re-resolves the stream: as
// long as the outlet is still discoverable, the inlet keeps waiting on the dead socket forever.
//
// The tests route the inlet's data connection through a local TCP relay that can "drop" its
// established connections: it keeps both sockets open but discards everything it reads in either
// direction, like a network path that black-holes packets. Connections made after the drop are
// relayed normally, i.e. the network has come back but the old connection's state is gone.
//
// Sets the api_config singleton, so (like runtime_config) it lives in its own executable.

using namespace std::chrono_literals;
using asio::ip::tcp;

namespace {

/// TCP relay that can black-hole its established connections without closing them.
class blackhole_relay {
	struct connection : std::enable_shared_from_this<connection> {
		tcp::socket client, upstream;
		std::atomic<bool> dropped{false};
		char to_upstream[16384], to_client[16384];

		connection(tcp::socket &&client, tcp::socket &&upstream)
			: client(std::move(client)), upstream(std::move(upstream)) {}

		void start() {
			pump(client, upstream, to_upstream);
			pump(upstream, client, to_client);
		}

		void pump(tcp::socket &from, tcp::socket &to, char *buf) {
			auto self = shared_from_this();
			from.async_read_some(asio::buffer(buf, sizeof(to_client)),
				[this, self, &from, &to, buf](const asio::error_code &ec, std::size_t n) {
					// A dropped connection must not pass on a close to the other side, otherwise
					// the peer would get a FIN and the connection wouldn't be half-open.
					if (ec) {
						if (!dropped) close();
						return;
					}
					// Dropped: swallow the data, as if the packets were lost on the way.
					if (dropped) return pump(from, to, buf);
					asio::async_write(to, asio::buffer(buf, n),
						[this, self, &from, &to, buf](const asio::error_code &ec, std::size_t) {
							if (ec) {
								if (!dropped) close();
							} else
								pump(from, to, buf);
						});
				});
		}

		void close() {
			asio::error_code ec;
			client.close(ec);
			upstream.close(ec);
		}
	};

	asio::io_context ctx_;
	tcp::acceptor acceptor_;
	tcp::endpoint target_;
	std::mutex mut_;
	std::vector<std::shared_ptr<connection>> connections_;
	std::thread thread_;

	void accept() {
		acceptor_.async_accept([this](const asio::error_code &ec, tcp::socket client) {
			if (ec) return;
			tcp::socket upstream(ctx_);
			asio::error_code connect_ec;
			upstream.connect(target_, connect_ec);
			if (!connect_ec) {
				auto conn = std::make_shared<connection>(std::move(client), std::move(upstream));
				{
					std::lock_guard<std::mutex> lock(mut_);
					connections_.push_back(conn);
				}
				conn->start();
			}
			accept();
		});
	}

public:
	explicit blackhole_relay(uint16_t target_port)
		: acceptor_(ctx_, tcp::endpoint(asio::ip::address_v4::loopback(), 0)),
		  target_(asio::ip::address_v4::loopback(), target_port) {
		accept();
		thread_ = std::thread([this]() { ctx_.run(); });
	}

	~blackhole_relay() {
		ctx_.stop();
		thread_.join();
	}

	uint16_t port() const { return acceptor_.local_endpoint().port(); }

	/// Number of connections accepted so far.
	std::size_t connection_count() {
		std::lock_guard<std::mutex> lock(mut_);
		return connections_.size();
	}

	/// Silently black-hole all currently established connections.
	void drop_established() {
		std::lock_guard<std::mutex> lock(mut_);
		for (auto &conn : connections_) conn->dropped = true;
	}
};

/// Pushes one-channel samples with increasing values at a fixed rate until destroyed.
class sample_pusher {
	std::atomic<bool> stop_{false};
	std::atomic<float> next_{0.f};
	std::thread thread_;

public:
	sample_pusher(lsl::stream_outlet_impl &outlet, double srate)
		: thread_([this, &outlet, srate]() {
			  const auto interval = std::chrono::duration<double>(1. / srate);
			  while (!stop_) {
				  std::vector<float> sample{next_.load()};
				  outlet.push_sample(sample, lsl::lsl_clock());
				  next_ = next_ + 1.f;
				  std::this_thread::sleep_for(interval);
			  }
		  }) {}
	~sample_pusher() {
		stop_ = true;
		thread_.join();
	}
	float next_value() const { return next_; }
};

/// The outlet's stream info with its data port replaced by the relay's port.
lsl::stream_info_impl info_via_relay(const lsl::stream_outlet_impl &outlet, uint16_t port) {
	lsl::stream_info_impl info(outlet.info());
	info.v4address("127.0.0.1");
	info.v4data_port(port);
	return info;
}

void configure() {
	// Short watchdog intervals so the tests run in seconds instead of the default ~30s
	lsl::api_config::set_api_config_content("[ports]\n"
											"IPv6 = disable\n"
											"[multicast]\n"
											"ResolveScope = machine\n"
											"MachineAddresses = {127.0.0.1}\n"
											"[lab]\n"
											"SessionID = inlet_stall_test\n"
											"KnownPeers = {}\n"
											"[tuning]\n"
											"WatchdogCheckInterval = 0.25\n"
											"WatchdogTimeThreshold = 1.0\n");
}

} // namespace

TEST_CASE("inlet reconnects after its data connection silently dies", "[inlet][recovery]") {
	configure();
	const double srate = 100.;
	lsl::stream_outlet_impl outlet(
		lsl::stream_info_impl("stalltest", "test", 1, srate, cft_float32, "stallsrc"));
	blackhole_relay relay(outlet.info().v4data_port());
	lsl::stream_inlet_impl inlet(info_via_relay(outlet, relay.port()), 360, 0, true);
	sample_pusher pusher(outlet, srate);

	std::vector<float> sample(1);
	inlet.open_stream(5.);
	REQUIRE(inlet.pull_sample(sample, 5.) != 0.);
	REQUIRE(relay.connection_count() == 1);

	// Kill the data connection without telling either side
	relay.drop_established();
	const float first_value_after_drop = pusher.next_value();

	// Samples that were already received may still be queued, so look for one that was pushed
	// after the drop. It can only arrive over a new connection.
	bool resumed = false;
	const auto deadline = std::chrono::steady_clock::now() + 15s;
	while (!resumed && std::chrono::steady_clock::now() < deadline)
		if (inlet.pull_sample(sample, 0.5) != 0. && sample[0] > first_value_after_drop)
			resumed = true;

	CHECK(resumed);
	CHECK(relay.connection_count() >= 2);
}

TEST_CASE("a quiet irregular-rate stream is not reconnected", "[inlet][recovery]") {
	configure();
	lsl::stream_outlet_impl outlet(lsl::stream_info_impl(
		"stalltest_irreg", "test", 1, lsl::IRREGULAR_RATE, cft_float32, "stallsrc_irreg"));
	blackhole_relay relay(outlet.info().v4data_port());
	lsl::stream_inlet_impl inlet(info_via_relay(outlet, relay.port()), 360, 0, true);

	std::vector<float> sample{1.f};
	inlet.open_stream(5.);
	outlet.push_sample(sample, lsl::lsl_clock());
	REQUIRE(inlet.pull_sample(sample, 5.) != 0.);

	// Gaps between samples are normal for irregular streams (e.g. event markers), so several
	// watchdog thresholds without data must not cause a reconnect that could lose samples
	std::this_thread::sleep_for(4s);
	CHECK(relay.connection_count() == 1);

	// ...and the original connection still delivers
	sample[0] = 2.f;
	outlet.push_sample(sample, lsl::lsl_clock());
	REQUIRE(inlet.pull_sample(sample, 5.) != 0.);
	CHECK(sample[0] == 2.f);
}

TEST_CASE(
	"a stream opened long after the inlet was created is not reconnected", "[inlet][recovery]") {
	configure();
	const double srate = 100.;
	lsl::stream_outlet_impl outlet(
		lsl::stream_info_impl("stalltest_late", "test", 1, srate, cft_float32, "stallsrc_late"));
	blackhole_relay relay(outlet.info().v4data_port());
	lsl::stream_inlet_impl inlet(info_via_relay(outlet, relay.port()), 360, 0, true);
	sample_pusher pusher(outlet, srate);

	// Idle for longer than the watchdog threshold before the data connection is opened
	std::this_thread::sleep_for(2s);
	std::vector<float> sample(1);
	inlet.open_stream(5.);
	const auto until = std::chrono::steady_clock::now() + 2s;
	while (std::chrono::steady_clock::now() < until) REQUIRE(inlet.pull_sample(sample, 1.) != 0.);
	CHECK(relay.connection_count() == 1);
}
