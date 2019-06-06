#define CATCH_CONFIG_MAIN
#include "gtest/gtest.h"
#include <lsl_cpp.h>

namespace {
template <typename T> void test_DataType(const char *name, lsl::channel_format_t cf) {
	const int numBounces = sizeof(T) * 8;
	double timestamps[numBounces][2];

	lsl::stream_outlet outlet(
		lsl::stream_info(name, "Bounce", 1, lsl::IRREGULAR_RATE, cf, "streamid"));
	auto found_stream_info = lsl::resolve_stream("name", name, 1, 5.0);

	EXPECT_FALSE(found_stream_info.empty());
	lsl::stream_info si = found_stream_info[0];

	lsl::stream_inlet inlet(si);

	inlet.open_stream(2);
	outlet.wait_for_consumers(2);

	T sent_data = 0x1;
	for (int32_t counter = 0; counter < numBounces; counter++) {
		T received_data;
		timestamps[counter][0] = lsl::local_clock();
		outlet.push_sample(&sent_data, timestamps[counter][0], true);

		EXPECT_NE(inlet.pull_sample(&received_data, 1, .5), 0.0);

		timestamps[counter][1] = lsl::local_clock();
		EXPECT_EQ(received_data, sent_data);
		sent_data = sent_data << 1;
	}
}

TEST(DataType, cf_int8) { test_DataType<char>("cf_int8", lsl::cf_int8); }
TEST(DataType, cf_int16) { test_DataType<int16_t>("cf_int16", lsl::cf_int16); }
TEST(DataType, cf_int32) { test_DataType<int32_t>("cf_int32", lsl::cf_int32); }
// TEST(DataType, cf_int64) { test_DataType<int64_t>("cf_int64", lsl::cf_int64); }

template <typename T> void test_DataTypeMulti(const char *name, lsl::channel_format_t cf) {
	const int numChannels = sizeof(T) * 8;

	lsl::stream_outlet outlet(
		lsl::stream_info(name, "Bounce", numChannels, lsl::IRREGULAR_RATE, cf, "streamid"));
	auto found_stream_info = lsl::resolve_stream("name", name, 1, 5.0);

	EXPECT_FALSE(found_stream_info.empty());
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
	EXPECT_NE(inlet.pull_sample(received_data, 0.5), 0.0);
	EXPECT_EQ(received_data, sent_data);
}

TEST(DataType, cf_int8_multichannel) { test_DataTypeMulti<char>("cf_int8", lsl::cf_int8); }
TEST(DataType, cf_int16_multichannel) { test_DataTypeMulti<int16_t>("cf_int16", lsl::cf_int16); }
TEST(DataType, cf_int32_multichannel) { test_DataTypeMulti<int32_t>("cf_int32", lsl::cf_int32); }
// TEST(DataType, cf_int64_multichannel) { test_DataTypeMulti<int64_t>("cf_int64", lsl::cf_int64); }

} // namespace
