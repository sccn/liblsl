#include "api_config.h"
#include "common.h"
#include "inlet_connection.h"
#include "stream_info_impl.h"
#include "stream_outlet_impl.h"
#include "time_receiver.h"
#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cmath>
#include <sstream>
#include <thread>
#include <vector>

// The drift history inside time_receiver must be dropped when an old (tokenless) outlet is
// snapped to 0 and when the inlet recovers to another outlet. Sets the api_config singleton, so
// (like time_config) it lives in its own executable.

using udp = asio::ip::udp;

namespace {

/// LSL:timedata responder like a liblsl without clock identity, whose clock reads
/// lsl_clock() + offset. It replies 1 ms late, so a measured offset is clearly not 0 (about
/// -0.5 ms) but still consistent with the same clock when offset is 0.
class fake_time_server {
public:
	explicit fake_time_server(double offset) : offset_(offset) {
		arm();
		thread_ = std::thread([this]() { ctx_.run(); });
	}
	~fake_time_server() { stop(); }
	void stop() {
		ctx_.stop();
		if (thread_.joinable()) thread_.join();
	}
	uint16_t port() const { return sock_.local_endpoint().port(); }

private:
	void arm() {
		sock_.async_receive_from(asio::buffer(buf_), from_, [this](asio::error_code ec, std::size_t n) {
			if (ec) return;
			const double t1 = lsl::lsl_clock() + offset_;
			std::istringstream is(std::string(buf_, n));
			std::string line;
			int wave = 0;
			double t0 = 0;
			std::getline(is, line);
			is >> wave >> t0;
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			std::ostringstream os;
			os.precision(16);
			os << ' ' << wave << ' ' << t0 << ' ' << t1 << ' ' << t1;
			sock_.send_to(asio::buffer(os.str()), from_);
			arm();
		});
	}
	double offset_;
	asio::io_context ctx_;
	asio::basic_datagram_socket<udp, asio::io_context::executor_type> sock_{
		ctx_, udp::endpoint(asio::ip::address_v4::loopback(), 0)};
	udp::endpoint from_;
	char buf_[512]{};
	std::thread thread_;
};

lsl::stream_info_impl fake_info(uint16_t service_port) {
	lsl::stream_info_impl info("drifttest", "Test", 1, LSL_IRREGULAR_RATE, cft_float32, "driftsrc");
	info.v4address("127.0.0.1");
	info.v4data_port(1);
	info.v4service_port(service_port);
	return info;
}

/// time_correction() every 0.25 s for the given duration
std::vector<double> poll(lsl::time_receiver &tr, double duration) {
	std::vector<double> res;
	for (double t_end = lsl::lsl_clock() + duration; lsl::lsl_clock() < t_end;) {
		double remote_time, uncertainty;
		res.push_back(tr.time_correction(&remote_time, &uncertainty, 5.0));
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}
	return res;
}

} // namespace

TEST_CASE("time_receiver drops the drift history", "[timesync][drift]") {
	// library defaults (TimeDriftWindow 60, same-clock detection and fallback on) except a 1.5 s
	// update interval: the fit then engages at the 5th wave (6 s span)
	lsl::api_config::set_api_config_content("[ports]\nIPv6 = disable\n"
											"[multicast]\nResolveScope = machine\n"
											"[tuning]\nTimeUpdateInterval = 1.5\n");
	REQUIRE(lsl::api_config::get_instance()->time_drift_window() == 60);

	SECTION("when a tokenless outlet on this machine is snapped to 0") {
		// waves 1-2 are measured (about -0.5 ms), wave 3 snaps; without clearing the history the
		// fit over waves 1-5 has a nonzero slope and returns a nonzero correction
		fake_time_server srv(0);
		lsl::inlet_connection conn(fake_info(srv.port()), false);
		lsl::time_receiver tr(conn);
		auto tc = poll(tr, 12);
		std::size_t first_zero = 0;
		while (first_zero < tc.size() && tc[first_zero] != 0.0) ++first_zero;
		CAPTURE(tc);
		REQUIRE(first_zero > 0); // the first waves are measured
		REQUIRE(first_zero + 30 < tc.size()); // ... and the fallback snaps soon after
		for (std::size_t i = first_zero; i < tc.size(); ++i) {
			CHECK(tc[i] == 0.0);
			CHECK_FALSE(std::signbit(tc[i]));
		}
	}

	SECTION("when the inlet recovers to another outlet") {
		// old outlet: clock 1000 s ahead; replacement: a real outlet in this process (offset 0)
		auto srv = std::make_unique<fake_time_server>(1000);
		lsl::stream_outlet_impl replacement(lsl::stream_info_impl(
			"drifttest", "Test", 1, LSL_IRREGULAR_RATE, cft_float32, "driftsrc"));
		lsl::inlet_connection conn(fake_info(srv->port()), true);
		lsl::time_receiver tr(conn);
		auto before = poll(tr, 9); // >= 5 waves over >= 6 s: the fit is in use
		REQUIRE(std::abs(before.back() + 1000) < 0.01);
		srv->stop(); // the old outlet is gone ...
		std::this_thread::sleep_for(std::chrono::seconds(2));
		conn.try_recover_from_error(); // ... and the inlet finds the replacement by source_id
		auto after = poll(tr, 5);
		CAPTURE(after);
		for (double v : after) CHECK(std::abs(v) < 0.01);
	}
}
