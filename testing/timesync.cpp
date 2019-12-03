#include "catch.hpp"
#include <lsl_cpp.h>
#include <iostream>

namespace {

TEST_CASE("simple timesync", "[timesync][basic]") {
	lsl::stream_outlet outlet(lsl::stream_info("timesync", "Test"));
	auto si = lsl::resolve_stream("name", "timesync", 1, 2.0);
	REQUIRE(si.size() == 1);
	lsl::stream_inlet inlet(si[0]);
	double offset = inlet.time_correction(5.) * 1000;
	CHECK(offset < 1);
	CAPTURE(offset);

	double remote_time, uncertainty;
	offset = inlet.time_correction(&remote_time, &uncertainty, 5.) * 1000;
	CHECK(offset < 1);
	CHECK(uncertainty * 1000 < 1);
	CHECK(remote_time < lsl::local_clock());
}

} // namespace
