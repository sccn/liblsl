#include "common.h"
#include <catch2/catch_test_macros.hpp>

// clazy:excludeall=non-pod-global-static

TEST_CASE("clock id", "[timesync][basic]") {
	const std::string id = lsl::lsl_clock_id();
	CAPTURE(id);
#if defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
	CHECK(id.size() == 16);
#endif
	CHECK(id.find_first_not_of("0123456789abcdef") == std::string::npos);
	CHECK(id == lsl::lsl_clock_id());
}
