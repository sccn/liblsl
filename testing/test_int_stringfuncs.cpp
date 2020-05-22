#include "../src/common.h"
#include "../src/inireader.h"
#include "catch.hpp"

using vec = std::vector<std::string>;

TEST_CASE("strings are split correctly", "[string][basic]") {
	CHECK(lsl::splitandtrim(" ", ',', true) == vec{""});
	CHECK(lsl::splitandtrim(" ", ',', false) == vec{});
	CHECK(lsl::splitandtrim(" , ", ',', true) == vec{"", ""});
	CHECK(lsl::splitandtrim(" , ", ',', false) == vec{});
	CHECK(lsl::splitandtrim(" a ", ',', false) == vec{"a"});
	CHECK(lsl::splitandtrim("a,b", ',', true) == vec{"a", "b"});
	CHECK(lsl::splitandtrim(",a,,", ',', false) == vec{"a"});
	CHECK(lsl::splitandtrim("a, b \t,\t c ", ',', true) == vec{"a", "b", "c"});
}
