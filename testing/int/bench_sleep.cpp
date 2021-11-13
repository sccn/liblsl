#include <catch2/catch.hpp>
#include <common.h>
#include <thread>

// clazy:excludeall=non-pod-global-static

TEST_CASE("sleep") {
	BENCHMARK("sleep1ms") { std::this_thread::sleep_for(std::chrono::milliseconds(1)); };
}

TEST_CASE("read system clock"){
	BENCHMARK("lsl_local_clock") { return lsl_local_clock(); };
	BENCHMARK("lsl_local_clock_ns") { return lsl::lsl_local_clock_ns(); };
}
