#include "helpers.h"
#include "catch.hpp"
#include <lsl_cpp.h>
#include <iostream>

namespace {

TEST_CASE("simple timesync", "[timesync][basic]") {
	auto sp = create_streampair(lsl::stream_info("timesync", "Test"));
	double offset = sp.in_.time_correction(5.) * 1000;
	CHECK(offset < 1);
	CAPTURE(offset);

	double remote_time, uncertainty;
	offset = sp.in_.time_correction(&remote_time, &uncertainty, 5.) * 1000;
	CHECK(offset < 1);
	CHECK(uncertainty * 1000 < 1);
	CHECK(remote_time < lsl::local_clock());
}

} // namespace
