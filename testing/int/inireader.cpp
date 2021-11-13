#include "util/inireader.hpp"
#include <catch2/catch.hpp>
#include <sstream>

// clazy:excludeall=non-pod-global-static

void try_load(INI &pt, const char *contents) {
	std::istringstream stream{std::string(contents)};
	pt.load(stream);
}

TEST_CASE("ini files are parsed correctly", "[ini][basic]") {
	INI pt;
	try_load(pt, "x=5\n"
				 "y=2\n"
				 "[foo]\n"
				 "foo=bar\n"
				 "; foo=commented out\n"
				 "double=equals=sign\n"
				 "[white space]\n"
				 "\tfoo  =\t bar\r\n");
	CHECK(pt.get("doesntexist", 0) == 0);
	CHECK(pt.get<int>("defaultval") == 0);
	CHECK(pt.get<int>("x") == 5);
	CHECK(pt.get("foo.foo", "") == std::string("bar"));
	CHECK(pt.get<const char*>("white space.foo") == std::string("bar"));
	CHECK(pt.get<const char*>("emptydefault") == std::string(""));
}

TEST_CASE("bad ini files are rejected", "[ini][basic]") {
	INI pt;
	CHECK_THROWS(try_load(pt, "[badsection"));
	CHECK_THROWS(try_load(pt, "duplicate=1\nduplicate=2"));
	CHECK_THROWS(try_load(pt, "missingval"));
	CHECK_THROWS(try_load(pt, "missingval= "));
	CHECK_THROWS(try_load(pt, " = missingkey"));
}
