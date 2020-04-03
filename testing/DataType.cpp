#include "catch.hpp"
#include "helpers.h"
#include "helper_type.hpp"
#include <cstdint>
#include <lsl_cpp.h>

TEMPLATE_TEST_CASE("datatransfer", "[datatransfer][basic]", char, int16_t, int32_t, int64_t, float, double) {
	const int numBounces = sizeof(TestType) * 8;
	double timestamps[numBounces][2];
	const char* name = SampleType<TestType>::fmt_string();
	lsl::channel_format_t cf = (lsl::channel_format_t) SampleType<TestType>::chan_fmt;

	Streampair sp(create_streampair(
		lsl::stream_info(name, "Bounce", 2, lsl::IRREGULAR_RATE, cf, "streamid")));
	lsl::stream_inlet &inlet = sp.in_;
	lsl::stream_outlet &outlet = sp.out_;

	inlet.open_stream(2);
	outlet.wait_for_consumers(2);

	TestType sent_data[2] = {0x1, 0x1};
	for (int32_t counter = 0; counter < numBounces; counter++) {
		TestType received_data[2];
		timestamps[counter][0] = lsl::local_clock();
		sent_data[1] = -sent_data[0] + 1;
		outlet.push_sample(sent_data, timestamps[counter][0], true);

		CHECK(inlet.pull_sample(received_data, 2, .5) != 0.0);

		timestamps[counter][1] = lsl::local_clock();
		CHECK(received_data[0] == sent_data[0]);
		CHECK(received_data[1] == sent_data[1]);
		sent_data[0] = static_cast<TestType>(static_cast<int64_t>(sent_data[0]) << 1);
	}
}

TEST_CASE("data datatransfer", "[datatransfer][multi][string]") {
	const std::size_t numChannels = 2;

	lsl::stream_outlet outlet(
		lsl::stream_info("cf_string", "DataType", numChannels, lsl::IRREGULAR_RATE, lsl::cf_string, "streamid"));
	auto found_stream_info = lsl::resolve_stream("name", "cf_string", 1, 5.0);

	REQUIRE(found_stream_info.size() > 0);
	lsl::stream_info si = found_stream_info[0];

	lsl::stream_inlet inlet(si);

	inlet.open_stream(2);
	outlet.wait_for_consumers(2);

	std::vector<std::string> sent_data, received_data(numChannels);
	const char nullstr[] = "\0Test\0string\0with\0nulls";
	sent_data.emplace_back(nullstr, sizeof(nullstr));
	sent_data.emplace_back(1 << 20, 'x');

	outlet.push_sample(sent_data);
	CHECK(inlet.pull_sample(received_data, 5.) != 0.0);
	CHECK(received_data[0] == sent_data[0]);
	if(received_data[1] != sent_data[1])
		FAIL("Sent large string data doesn't match received data");
}

TEST_CASE("data typeconversion", "[datatransfer][types][basic]") {
	Streampair sp{
		create_streampair(lsl::stream_info("TypeConversion", "int2str2int", 1, 1, lsl::cf_string))};
	sp.in_.open_stream(2);
	const int num_bounces = 31;
	std::vector<int32_t> data;
	for (int i = 0; i < num_bounces; ++i) data.push_back(1 << i);
	sp.out_.push_chunk_multiplexed(data);
	for (int i = 0; i < num_bounces; ++i) {
		int32_t result;
		sp.in_.pull_sample(&result, 1, 1.);
		CHECK(result == 1 << i);
	}
}

