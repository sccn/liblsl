#include "gtest/gtest.h"
#include <lsl_cpp.h>

namespace {

TEST(Resolve, multi) {
	lsl::continuous_resolver resolver("type", "Resolve");
	std::vector<lsl::stream_outlet> outlets;
	const int n = 3;
	for (int i = 0; i < n; i++)
		outlets.emplace_back(lsl::stream_info("resolvetest_" + std::to_string(i), "Resolve"));
	auto found_stream_info = lsl::resolve_stream("type", "Resolve", n, 2.0);
	ASSERT_EQ(found_stream_info.size(), n);
	ASSERT_EQ(resolver.results().size(), n);
}

TEST(Resolve, from_streaminfo) {
	lsl::stream_outlet outlet(lsl::stream_info("resolvetest", "from_streaminfo"));
	lsl::stream_inlet(outlet.info());
}

} // namespace
