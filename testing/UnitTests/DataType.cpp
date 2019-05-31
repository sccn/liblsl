#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include <lsl_cpp.h>

template <typename T> void test_DataType(const char *name, lsl::channel_format_t cf) {
	std::cout << name << ", [DataType]" << std::endl;
	const int numBounces = sizeof(T) * 8;
	double timestamps[numBounces][2];

	lsl::stream_outlet outlet(
		lsl::stream_info(name, "Bounce", 1, lsl::IRREGULAR_RATE, cf, "streamid"));
	auto found_stream_info = lsl::resolve_stream("name", name, 1, 5.0);

	REQUIRE(!found_stream_info.empty());
	lsl::stream_info si = found_stream_info[0];

	lsl::stream_inlet inlet(si);

	inlet.open_stream(2);
	outlet.wait_for_consumers(2);

	T sent_data = 0x1;
	for (int32_t counter = 0; counter < numBounces; counter++) {
		T received_data;
		timestamps[counter][0] = lsl::local_clock();
		outlet.push_sample(&sent_data, timestamps[counter][0], true);

		REQUIRE(inlet.pull_sample(&received_data, 1, .5) != 0.0);

		timestamps[counter][1] = lsl::local_clock();
		REQUIRE(received_data == sent_data);
		sent_data = sent_data << 1;
	}
}

// Confirm that cf_int8 is preserved during transmission
TEST_CASE("cf_int8", "[DataType]") { test_DataType<char>("cf_int8", lsl::cf_int8); }


// Confirm that cf_int16 is preserved during transmission
TEST_CASE("cf_int16", "[DataType]") { test_DataType<int16_t>("cf_int16", lsl::cf_int16); }


// Confirm that cf_int32 is preserved during transmission
TEST_CASE("cf_int32", "[DataType]") { test_DataType<int32_t>("cf_int32", lsl::cf_int32); }

/*
//Confirm that cf_int64 is preserved during transmission
TEST_CASE("cf_int64", "[DataTye]") {
	test_DataType<int64_t>("cf_int64", lsl::cf_int64);
}
*/

template <typename T> void test_DataTypeMulti(const char *name, lsl::channel_format_t cf) {
	std::cout << name << " multichannel, [DataType]" << std::endl;
	const int numChannels = sizeof(T) * 8;

	lsl::stream_outlet outlet(
		lsl::stream_info(name, "Bounce", numChannels, lsl::IRREGULAR_RATE, cf, "streamid"));
	auto found_stream_info = lsl::resolve_stream("name", name, 1, 5.0);

	REQUIRE(!found_stream_info.empty());
	lsl::stream_info si = found_stream_info[0];

	lsl::stream_inlet inlet(si);

	inlet.open_stream(2);
	outlet.wait_for_consumers(2);

	std::vector<T> sent_data(numChannels);
	T data = 0x1LL;
	for (uint32_t counter = 0; counter < numChannels; counter++) {
		sent_data[counter] = data;
		data = data << 1LL;
	}

	std::vector<T> received_data(numChannels);
	outlet.push_sample(sent_data, lsl::lsl_local_clock(), true);
	REQUIRE(inlet.pull_sample(received_data, 0.5) != 0.0);
	REQUIRE(received_data == sent_data);
}

// Confirm that cf_int8 multichannel is preserved during transmission
TEST_CASE("cf_int8 multichannel", "[DataTye]") {
	test_DataTypeMulti<char>("cf_int8", lsl::cf_int8);
}


// Confirm that cf_int16 multichannel is preserved during transmission
TEST_CASE("cf_int16 multichannel", "[DataTye]") {
	test_DataTypeMulti<int16_t>("cf_int16", lsl::cf_int16);
}


// Confirm that cf_int32 multichannel is preserved during transmission
TEST_CASE("cf_int32 multichannel", "[DataTye]") {
	test_DataTypeMulti<int32_t>("cf_int32", lsl::cf_int32);
}


/*
//Confirm that cf_int64 multichannel is preserved during transmission
TEST_CASE("cf_int64 multichannel", "[DataTye]") {
	test_DataTypeMulti<int64_t>("cf_int64", lsl::cf_int64);
}
*/
