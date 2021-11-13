#include "util/strfuns.hpp"
#include <catch2/catch.hpp>

// clazy:excludeall=non-pod-global-static

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

TEST_CASE("trim functions", "[string][basic]") {
	std::string testcase = "\nHello World\t\n  123";
	CHECK(*lsl::trim_begin(testcase.begin(), testcase.end()) == 'H');
	CHECK(*lsl::trim_end(testcase.begin(), testcase.begin() + 16) == '\t');
	std::string trimmed = "Hello World";
	CHECK(lsl::trim_begin(trimmed.begin(), trimmed.end()) == trimmed.begin());
	CHECK(lsl::trim_end(trimmed.begin(), trimmed.end()) == trimmed.end());
	const char test[] = "Hello World";
	CHECK(*lsl::trim_end(test, test + sizeof(test) - 1) == 0);

	auto begin = testcase.begin(), end = testcase.end();
	lsl::trim(begin, end);
	CHECK(*begin == 'H');
	CHECK(*(end - 1) == '3');

	CHECK(lsl::trim(testcase) == "Hello World\t\n  123");
	CHECK(lsl::trim("") == "");
}
