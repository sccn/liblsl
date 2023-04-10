#include <catch2/catch_all.hpp>
#include <lsl_c.h>

// clazy:excludeall=non-pod-global-static

TEST_CASE("common") {
	BENCHMARK("lsl_clock") { return lsl_local_clock(); };
}
