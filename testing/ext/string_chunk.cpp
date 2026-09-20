#include "../common/create_streampair.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <string>

// clazy:excludeall=non-pod-global-static

TEST_CASE("pull_chunk_str allocates only filled slots", "[datatransfer][string][chunk]") {
	const int channels = GENERATE(1, 2);
	Streampair sp(create_streampair(lsl::stream_info("string_chunk", "DataType", channels,
		lsl::IRREGULAR_RATE, lsl::cf_string, "string_chunk")));
	const std::vector<std::string> sent =
		channels == 1 ? std::vector<std::string>{"marker", ""}
					  : std::vector<std::string>{"marker", "", "second", "channel"};
	sp.out_.push_chunk_multiplexed(sent);

	std::array<char *, 1024> data{};
	int32_t ec = lsl_internal_error;
	const auto result =
		lsl_pull_chunk_str(sp.in_.handle().get(), data.data(), nullptr, data.size(), 0, 0.5, &ec);
	CHECK(ec == lsl_no_error);
	CHECK(result == sent.size());
	for (std::size_t k = 0; k < sent.size(); k++) {
		CHECK(data[k] != nullptr);
		if (data[k]) CHECK(std::string(data[k]) == sent[k]);
	}
	for (std::size_t k = result; k < data.size(); k++) CHECK(data[k] == nullptr);
	lsl_destroy_string_array(data.data(), data.size());
}

TEST_CASE("pull_chunk_buf preserves binary strings and clears unfilled slots",
	"[datatransfer][string][chunk]") {
	Streampair sp(create_streampair(lsl::stream_info(
		"binary_chunk", "DataType", 2, lsl::IRREGULAR_RATE, lsl::cf_string, "binary_chunk")));
	const std::vector<std::string> sent{"marker", "", std::string("\0a\0b\0", 5), "last"};
	sp.out_.push_chunk_multiplexed(sent);

	std::array<char *, 1024> data{};
	std::array<uint32_t, 1024> lengths;
	lengths.fill(42);
	int32_t ec = lsl_internal_error;
	const auto result = lsl_pull_chunk_buf(
		sp.in_.handle().get(), data.data(), lengths.data(), nullptr, data.size(), 0, 0.5, &ec);
	CHECK(ec == lsl_no_error);
	CHECK(result == sent.size());
	for (std::size_t k = 0; k < sent.size(); k++) {
		CHECK(data[k] != nullptr);
		CHECK(lengths[k] == sent[k].size());
		if (data[k]) CHECK(std::string(data[k], lengths[k]) == sent[k]);
	}
	for (std::size_t k = result; k < data.size(); k++) {
		CHECK(data[k] == nullptr);
		CHECK(lengths[k] == 0);
	}
	lsl_destroy_string_array(data.data(), data.size());
}

TEST_CASE("empty string chunk pulls clear every slot", "[datatransfer][string][chunk]") {
	Streampair sp(create_streampair(lsl::stream_info(
		"empty_chunk", "DataType", 1, lsl::IRREGULAR_RATE, lsl::cf_string, "empty_chunk")));
	char sentinel = 'x';
	std::array<char *, 1024> data;
	data.fill(&sentinel);
	std::array<uint32_t, 1024> lengths;
	lengths.fill(42);
	int32_t ec = lsl_internal_error;
	unsigned long result;
	SECTION("str") {
		result = lsl_pull_chunk_str(
			sp.in_.handle().get(), data.data(), nullptr, data.size(), 0, 0.0, &ec);
	}
	SECTION("buf") {
		result = lsl_pull_chunk_buf(
			sp.in_.handle().get(), data.data(), lengths.data(), nullptr, data.size(), 0, 0.0, &ec);
		for (auto length : lengths) CHECK(length == 0);
	}
	CHECK(result == 0);
	CHECK(ec == lsl_no_error);
	for (auto &entry : data) {
		CHECK(entry == nullptr);
		// Keep cleanup safe even if a regression leaves the sentinel untouched.
		if (entry == &sentinel) entry = nullptr;
	}
	lsl_destroy_string_array(data.data(), data.size());
}

TEST_CASE("lsl_destroy_string_array accepts zero count and NULL entries", "[string][chunk]") {
	lsl_destroy_string_array(nullptr, 0);
	char sentinel = 'x';
	char *untouched = &sentinel;
	lsl_destroy_string_array(&untouched, 0);
	CHECK(untouched == &sentinel);
	std::array<char *, 8> data{};
	lsl_destroy_string_array(data.data(), data.size());
	for (auto entry : data) CHECK(entry == nullptr);
}
