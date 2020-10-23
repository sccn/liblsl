#include "catch.hpp"
#include <lsl_c.h>

TEST_CASE("common") {
	BENCHMARK("lsl_clock") { return lsl_local_clock(); };
}
