#include <catch2/catch_all.hpp>
#include <chrono>
#include <lsl_cpp.h>
#include <thread>

// clazy:excludeall=non-pod-global-static

namespace {

bool wait_until_no_consumers(lsl::stream_outlet &outlet, double timeout_sec) {
	auto deadline =
		std::chrono::steady_clock::now() + std::chrono::duration<double>(timeout_sec);
	while (outlet.have_consumers() && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	return !outlet.have_consumers();
}

TEST_CASE("have_consumers becomes false after idle inlet disconnects", "[outlet][basic]") {
	lsl::stream_info info("have_consumers_idle", "Markers", 1, lsl::IRREGULAR_RATE, lsl::cf_int32,
		"have_consumers_idle");
	lsl::stream_outlet outlet(info);
	auto found = lsl::resolve_stream("name", info.name(), 1, 2.0);
	REQUIRE(found.size() == 1);

	{
		lsl::stream_inlet inlet(found[0]);
		inlet.open_stream(2);
		REQUIRE(outlet.wait_for_consumers(2));
		REQUIRE(outlet.have_consumers());
		// Intentionally do not push samples; disconnect must still be detected (#267).
	}

	CHECK(wait_until_no_consumers(outlet, 2.0));
}

} // namespace
