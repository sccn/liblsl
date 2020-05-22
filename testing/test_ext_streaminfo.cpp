#include "catch.hpp"
#include <lsl_cpp.h>

namespace {

TEST_CASE("streaminfo", "[streaminfo][basic]") {
	CHECK_THROWS(lsl::stream_info("", "emptyname"));
	CHECK(std::string("The name of a stream must be non-empty.") == lsl_last_error());
}
} // namespace
