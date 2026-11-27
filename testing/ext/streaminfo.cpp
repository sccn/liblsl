#include <catch2/catch_all.hpp>
#include <lsl_cpp.h>
#include <thread>

// clazy:excludeall=non-pod-global-static

namespace {

TEST_CASE("streaminfo", "[streaminfo][basic]") {
	CHECK_THROWS(lsl::stream_info("", "emptyname"));
	REQUIRE(std::string("The name of a stream must be non-empty.") == lsl_last_error());
}

TEST_CASE("multithreaded lsl_last_error", "[threading][basic]") {
	CHECK_THROWS(lsl::stream_info("", "emptyname"));
	std::thread([](){
		CHECK_THROWS(lsl::stream_info("hasname","type", -1));
		REQUIRE(std::string("The channel_count of a stream must be nonnegative.") == lsl_last_error());
	}).join();
	REQUIRE(std::string("The name of a stream must be non-empty.") == lsl_last_error());
}

TEST_CASE("streaminfo rejects single quotes in properties", "[streaminfo][basic]") {
	// Stream properties are embedded in single-quoted XPath literals when resolving streams,
	// so an apostrophe would make the stream impossible to resolve by property/recover (#325).
	CHECK_THROWS(lsl::stream_info("Bob's EEG", "EEG"));
	REQUIRE(std::string("The name of a stream must not contain single quotes.") ==
			lsl_last_error());

	CHECK_THROWS(lsl::stream_info("Bob", "EEG's"));
	REQUIRE(std::string("The type of a stream must not contain single quotes.") ==
			lsl_last_error());

	CHECK_THROWS(lsl::stream_info("Bob", "EEG", 1, lsl::IRREGULAR_RATE, lsl::cf_float32, "src's"));
	REQUIRE(std::string("The source_id of a stream must not contain single quotes.") ==
			lsl_last_error());

	// The C API reports the same error as a NULL return value
	CHECK(lsl_create_streaminfo("Bob's EEG", "EEG", 1, 0.0, cft_float32, "src") == nullptr);
	REQUIRE(std::string("The name of a stream must not contain single quotes.") ==
			lsl_last_error());

	// Regular names are still accepted
	CHECK_NOTHROW(lsl::stream_info("Bobs EEG", "EEG"));
}

TEST_CASE("streaminfo accepts single quotes in received info", "[streaminfo][basic]") {
	// Stream infos from (older) outlets are received over the network and must not be rejected.
	lsl::stream_info original("OK", "EEG", 1, 100.0, lsl::cf_float32, "src");
	original.reset_uid();
	std::string message = original.as_xml();
	auto pos = message.find(">OK<");
	REQUIRE(pos != std::string::npos);
	message.replace(pos, 4, ">Bob's EEG<");

	lsl_streaminfo handle = lsl_streaminfo_from_xml(message.c_str());
	REQUIRE(handle != nullptr);
	lsl::stream_info received(handle);
	REQUIRE(received.name() == "Bob's EEG");
}

/// Ensure that an overly long error message won't overflow the buffer
TEST_CASE("lsl_last_error size", "[basic]") {
	std::string invalidquery(511, '\'');
	CHECK_THROWS(lsl::resolve_stream(invalidquery, 1, 0.1));
	REQUIRE(lsl_last_error()[511] == 0);
}
} // namespace
