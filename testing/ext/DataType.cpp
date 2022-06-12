#include "../common/create_streampair.hpp"
#include "../common/lsltypes.hpp"
#include <catch2/catch.hpp>
#include <cstdint>
#include <lsl_cpp.h>
#include <thread>

// clazy:excludeall=non-pod-global-static

TEMPLATE_TEST_CASE(
	"datatransfer", "[datatransfer][basic]", char, int16_t, int32_t, int64_t, float, double) {
	const int numBounces = sizeof(TestType) * 8;
	double timestamps[numBounces][2];
	const char *name = SampleType<TestType>::fmt_string();
	auto cf = static_cast<lsl::channel_format_t>(SampleType<TestType>::chan_fmt);

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
		CHECK(received_data[0] == Approx(sent_data[0]));
		CHECK(received_data[1] == Approx(sent_data[1]));
		sent_data[0] = static_cast<TestType>(static_cast<int64_t>(sent_data[0]) << 1);
	}
}

TEST_CASE("data datatransfer", "[datatransfer][multi][string]") {
	const std::size_t numChannels = 3;

	Streampair sp(create_streampair(lsl::stream_info(
		"cf_string", "DataType", numChannels, lsl::IRREGULAR_RATE, lsl::cf_string, "streamid")));

	std::vector<std::string> sent_data, received_data(numChannels);
	const char nullstr[] = "\0Test\0string\0with\0nulls";
	sent_data.emplace_back(nullstr, sizeof(nullstr));
	sent_data.emplace_back(200, 'x');
	sent_data.emplace_back(1 << 20, 'x');

	sp.out_.push_sample(sent_data);
	CHECK(sp.in_.pull_sample(received_data, 5.) != 0.0);
	CHECK(received_data[0] == sent_data[0]);
	// Manually check second string for equality to avoid printing the entire test string
	if (received_data[1] != sent_data[1])
		FAIL("Sent large string data doesn't match received data");
}

TEST_CASE("TypeConversion", "[datatransfer][types][basic]") {
	Streampair sp{create_streampair(
		lsl::stream_info("TypeConversion", "int2str2int", 1, 1, lsl::cf_string, "TypeConversion"))};
	const int num_bounces = 31;
	std::vector<int32_t> data(num_bounces);
	for (int i = 0; i < num_bounces; ++i) data[i] = 1 << i;

	sp.out_.push_chunk_multiplexed(data);

	for (int32_t val : data) {
		int32_t result;
		sp.in_.pull_sample(&result, 1, 1.);
		CHECK(result == val);
	}
}

TEST_CASE("Flush", "[datatransfer][basic]") {
	Streampair sp{create_streampair(
		lsl::stream_info("FlushTest", "flush", 1, 1, lsl::cf_double64, "FlushTest"))};
	sp.in_.set_postprocessing(lsl::post_dejitter);

	const int n=20;

	std::thread pusher([&](){
		double data = lsl::local_clock();
		for(int i=0; i<n; ++i) {
			sp.out_.push_sample(&data, data, true);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			data+=.1;
		}
	});

	double data_in;
	double ts_in = sp.in_.pull_sample(&data_in, 1);
	REQUIRE(ts_in == Approx(data_in));
	std::this_thread::sleep_for(std::chrono::milliseconds(700));
	auto pulled = sp.in_.flush() + 1;

	for(; pulled < n; ++pulled) {
		INFO(pulled);
		ts_in = sp.in_.pull_sample(&data_in, 1);
		REQUIRE(ts_in == Approx(data_in));
	}

	pusher.join();
	//sp.in_.set_postprocessing(lsl::post_none);
}
