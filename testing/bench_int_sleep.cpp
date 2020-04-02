#include "catch.hpp"
#include <thread>

TEST_CASE("sleep") {
	BENCHMARK("sleep1ms") { std::this_thread::sleep_for(std::chrono::milliseconds(1)); };
}
