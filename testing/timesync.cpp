#include "gtest/gtest.h"
#include <lsl_cpp.h>

namespace {

TEST(timesync, simple) {
	lsl::stream_outlet outlet(lsl::stream_info("timesync", "Test"));
	auto si = lsl::resolve_stream("name", "timesync", 1, 2.0);
	ASSERT_EQ(si.size(), 1);
	lsl::stream_inlet inlet(si[0]);
	double offset = inlet.time_correction(5.);
	ASSERT_LE(offset, 0.001);
	std::cout << "\tOffset (ms): " << (offset*1000) << '\n';

	double remote_time, uncertainty;
	offset = inlet.time_correction(&remote_time, &uncertainty, 5.);
	ASSERT_LE(offset, 0.001);
	ASSERT_LE(remote_time, lsl::local_clock());
	std::cout << "\tOffset (ms): " << (offset * 1000) << "±" << (uncertainty * 1000) << '\n';
}

} // namespace
