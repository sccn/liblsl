#include "api_config.h"
#include "util/inireader.hpp"
#include <catch2/catch.hpp>
#include <sstream>

namespace lsl {
void test_api_config() {
	lsl::api_config cfg(true);

	// default settings have to be applied in any case
	REQUIRE(cfg.base_port_ == 16572);

	cfg.set_option("tuning", "ReceiveSocketBufferSize", "100");
	REQUIRE(cfg.socket_receive_buffer_size_ == 100);

	CHECK_FALSE(cfg.set_option("invalid", "option", ""));
	CHECK_THROWS(cfg.set_option("log", "level", "-100"));
	CHECK_THROWS(cfg.set_option("log", "level", "not a number"));
}
}

TEST_CASE("api_config", "[basic]") {
	lsl::test_api_config();
}

void try_load(const char *contents) {
	auto callback = [](const std::string&, const std::string&, const std::string&){};
	std::istringstream stream{std::string(contents)};
	load_ini_file(stream, callback);
}

TEST_CASE("ini files are parsed correctly", "[ini][basic]") {
	std::map<std::string, std::string> cfg;
	std::string ini{"x=5\n"
					"y=2\n"
					"[foo]\n"
					"foo=bar\n"
					"; foo=commented out\n"
					"double=equals=sign\n"
					"[white space]\n"
					"\tfoo  =\t bar\r\n"};
	std::istringstream stream{std::string(ini)};
	load_ini_file(
		stream, [&cfg](const std::string &key, const std::string &section,
					const std::string &value) { cfg[key + '.' + section] = value; });
	CHECK(cfg["foo.foo"] == std::string("bar"));
	CHECK(cfg["white space.foo"] == std::string("bar"));
}

TEST_CASE("bad ini files are rejected", "[ini][basic]") {
	CHECK_THROWS(try_load("[badsection"));
	CHECK_THROWS(try_load("duplicate=1\nduplicate=2"));
	CHECK_THROWS(try_load("missingval"));
	CHECK_THROWS(try_load("missingval= "));
	CHECK_THROWS(try_load(" = missingkey"));
}
