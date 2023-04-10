#include <catch2/catch_all.hpp>
#include <lsl_cpp.h>
#include <thread>

// clazy:excludeall=non-pod-global-static

namespace {

TEST_CASE("move C++ API types", "[move][basic]") {
	lsl::stream_info info("movetest", "test", 1, lsl::IRREGULAR_RATE, lsl::cf_int32);
	lsl::stream_outlet outlet(info);
	lsl::continuous_resolver resolver("name", "movetest");
	auto found_stream_info = lsl::resolve_stream("name", "movetest", 1, 2.0);
	REQUIRE(found_stream_info.size() == 1);
	lsl::stream_inlet inlet(found_stream_info[0]);

	inlet.open_stream(2);
	outlet.wait_for_consumers(2);

	int32_t data = 1;

	{
		// move the outlet via the move constructor
		lsl::stream_outlet outlet2(std::move(outlet));
		CHECK(outlet2.have_consumers());
		outlet2.push_sample(&data);
		// Move outlet2 back into outlet via the copy constructor
		outlet = std::move(outlet2);
		data++;
		outlet.push_sample(&data);
		// End of scope, destructor for outlet2 is called
		// Since the stream_outlet is alive in outlet, it's not deconstructed
	}

	{
		// move the inlet via the move constructor
		lsl::stream_inlet inlet2(std::move(inlet));
		REQUIRE(inlet2.get_channel_count() == 1);
		inlet2.pull_sample(&data, 1);
		CHECK(data == 1);
		// Move inlet2 back into inlet via the copy constructor
		inlet = std::move(inlet2);
		inlet.pull_sample(&data, 1);
		CHECK(data == 2);
		// End of scope, destructor for inlet2 is called
		// Since the stream_inlet is alive in inlet, it's not deconstructed
	}

	{
		lsl::continuous_resolver resolver2(std::move(resolver));
		resolver2.results();
		resolver = std::move(resolver2);
		resolver.results();
	}

	{
		lsl::stream_outlet outlet3(std::move(outlet));
		lsl::stream_inlet inlet3(std::move(inlet));
		// End of scope, destructors for inlet3 and outlet3 are called
	}

	// Since the outlet has been destructed in the previous block, it shouldn't
	// be there anymore
	found_stream_info = lsl::resolve_stream("name", "movetest", 1, 2.0);
	REQUIRE(found_stream_info.empty());

	// stream_info copies are cheap, for independent copies clone() has to be used
	lsl::stream_info copy1(info), copy2(info), clone(info.clone());
	copy1.desc().append_child("Dummy");
	REQUIRE(copy2.desc().first_child().name() == std::string("Dummy"));
	REQUIRE(clone.desc().first_child().empty());
}

} // namespace
