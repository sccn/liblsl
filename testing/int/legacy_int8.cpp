#include "../../src/legacy/legacy_abi.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>

// Keep this in a separate executable: the legacy and modern wrappers define
// different classes with the same names and must not share a test executable.
TEST_CASE("legacy char overloads preserve signed bytes", "[legacy][int8]") {
	lsl::stream_info info("legacy_int8", "DataType", 4, 0.0, cft_int32, "legacy_int8");
	lsl::stream_outlet outlet(info);
	auto found = lsl::resolve_stream("name", "legacy_int8", 1, 2.0);
	REQUIRE(found.size() == 1);
	lsl::stream_inlet inlet(found[0]);
	inlet.open_stream(2.0);
	REQUIRE(outlet.wait_for_consumers(2.0));

	const int8_t expected[] = {-128, -1, 0, 127};
	char sent[4];
	std::memcpy(sent, expected, sizeof(sent));
	int received[4]{};

	SECTION("pointer overloads") {
		outlet.push_sample(sent);
		REQUIRE(inlet.pull_sample(received, 4, 2.0) != 0.0);
		for (int i = 0; i < 4; ++i) CHECK(received[i] == expected[i]);
		outlet.push_sample(received);
		char bytes[4]{};
		REQUIRE(inlet.pull_sample(bytes, 4, 2.0) != 0.0);
		CHECK(std::memcmp(bytes, expected, sizeof(bytes)) == 0);
	}
	SECTION("vector overloads") {
		std::vector<char> bytes(sent, sent + 4);
		outlet.push_sample(bytes);
		REQUIRE(inlet.pull_sample(received, 4, 2.0) != 0.0);
		for (int i = 0; i < 4; ++i) CHECK(received[i] == expected[i]);
		outlet.push_sample(received);
		bytes.clear();
		REQUIRE(inlet.pull_sample(bytes, 2.0) != 0.0);
		REQUIRE(bytes.size() == 4);
		CHECK(std::memcmp(bytes.data(), expected, sizeof(expected)) == 0);
		bytes.resize(3);
		CHECK_THROWS_AS(outlet.push_sample(bytes), std::range_error);
	}
}
