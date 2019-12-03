#include "catch.hpp"
#include <lsl_cpp.h>
#include <thread>

namespace {

TEST_CASE("resolve multiple streams", "[resolver][basic]") {
	lsl::continuous_resolver resolver("type", "Resolve", 50.);
	std::vector<lsl::stream_outlet> outlets;
	const int n = 3;
	for (int i = 0; i < n; i++)
		outlets.emplace_back(lsl::stream_info("resolvetest_" + std::to_string(i), "Resolve"));
	auto found_stream_info = lsl::resolve_stream("type", "Resolve", n, 2.0);
	REQUIRE(found_stream_info.size() == n);

	// Allow the resolver a bit more time in case the first resolve wave was too fast
	std::this_thread::sleep_for(std::chrono::seconds(1));
	REQUIRE(resolver.results().size() == n);
}

TEST_CASE("resolve from streaminfo", "[resolver][streaminfo][basic]") {
	lsl::stream_outlet outlet(lsl::stream_info("resolvetest", "from_streaminfo"));
	lsl::stream_inlet(outlet.info());
}

} // namespace
