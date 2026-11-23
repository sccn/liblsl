#include "api_config.h"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <lsl_cpp.h>

// Sets the api_config singleton, so (like runtime_config) it lives in its own executable.

TEST_CASE("time correction with a short TimeUpdateInterval", "[timesync][time_config]") {
	// 0.3 s is shorter than the 0.64 s aggregation delay; this used to cancel every aggregation
	lsl::api_config::set_api_config_content("[ports]\n"
											"IPv6 = disable\n"
											"[multicast]\n"
											"ResolveScope = machine\n"
											"[tuning]\n"
											"TimeUpdateInterval = 0.3\n"
											"TimeSameClockDetection = 0\n");
	lsl::stream_outlet out(lsl::stream_info("timecfg", "Test", 1, lsl::IRREGULAR_RATE));
	auto found = lsl::resolve_stream("name", "timecfg", 1, 5.0);
	REQUIRE(!found.empty());
	lsl::stream_inlet in(found[0]);
	double remote_time, uncertainty;
	double offset = in.time_correction(&remote_time, &uncertainty, 3.0);
	CHECK(std::abs(offset) < 0.001);
	CHECK(uncertainty > 0); // measured, not same-clock
	CHECK(uncertainty < 0.01);
}
