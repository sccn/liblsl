#include "gtest/gtest.h"
#include <lsl_cpp.h>

namespace {

TEST(timesync, simple) {
	lsl::stream_outlet outlet(lsl::stream_info("timesync", "Test"));
	auto si = lsl::resolve_stream("type", "Test", 5, 2.0);
	ASSERT_EQ(si.size(), 1);
	lsl::stream_inlet inlet(si[0]);
	double remote_time, uncertainty;
	double offset = inlet.time_correction(&remote_time, &uncertainty, 5.);

	ASSERT_LE(offset, 0.01);
	ASSERT_LE(remote_time, lsl::local_clock());
}


} // namespace
