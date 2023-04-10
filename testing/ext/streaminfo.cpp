#include <catch2/catch_all.hpp>
#include <lsl_cpp.h>
#include <thread>

// clazy:excludeall=non-pod-global-static

namespace {

TEST_CASE("streaminfo", "[streaminfo][basic]") {
	CHECK_THROWS(lsl::stream_info("", "emptyname"));
	REQUIRE(std::string("The name of a stream must be non-empty.") == lsl_last_error());
}

TEST_CASE("multithreaded lsl_last_error", "[threading][basic]") {
	CHECK_THROWS(lsl::stream_info("", "emptyname"));
	std::thread([](){
		CHECK_THROWS(lsl::stream_info("hasname","type", -1));
		REQUIRE(std::string("The channel_count of a stream must be nonnegative.") == lsl_last_error());
	}).join();
	REQUIRE(std::string("The name of a stream must be non-empty.") == lsl_last_error());
}

/// Ensure that an overly long error message won't overflow the buffer
TEST_CASE("lsl_last_error size", "[basic]") {
	std::string invalidquery(511, '\'');
	CHECK_THROWS(lsl::resolve_stream(invalidquery, 1, 0.1));
	REQUIRE(lsl_last_error()[511] == 0);
}
} // namespace
