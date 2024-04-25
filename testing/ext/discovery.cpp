#include <catch2/catch_all.hpp>
#include <lsl_cpp.h>
#include <thread>

// clazy:excludeall=non-pod-global-static

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

TEST_CASE("Invalid queries are caught before sending the query", "[resolver][streaminfo][basic]") {
	REQUIRE_THROWS(lsl::resolve_stream("invalid'query", 0, 0.1));
}

TEST_CASE("fullinfo", "[inlet][fullinfo][basic]") {
	lsl::stream_info info("fullinfo", "unittest", 1, 1, lsl::cf_int8, "fullinfo1234");
	const std::string extinfo("contents\nwith\n\tnewlines");
	info.desc().append_child_value("info", extinfo);
	lsl::stream_outlet outlet(info);
	auto found_streams = lsl::resolve_stream("name", info.name(), 1, 2);
	REQUIRE(!found_streams.empty());
	INFO(found_streams[0].as_xml());
	CHECK(found_streams[0].desc().first_child().empty());
	auto fullinfo = lsl::stream_inlet(found_streams[0]).info(2);
	INFO(fullinfo.as_xml());
	CHECK(fullinfo.desc().child_value("info") == extinfo);
}

TEST_CASE("downed outlet deadlock", "[inlet][streaminfo]")
{
	// This test verifies that calling info on a resolved inlet that has become disconnected
	// does not get locked waiting on a response.
	auto outlet = std::make_unique<lsl::stream_outlet>(lsl::stream_info("deadtest", "type"));

	auto resolved = lsl::resolve_streams();
	REQUIRE(!resolved.empty());
	lsl::stream_inlet inlet(resolved[0]);

	outlet.reset();

	// this would previously deadlock
	CHECK_THROWS(inlet.info());
}


} // namespace
