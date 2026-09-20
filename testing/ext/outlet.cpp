#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <lsl_cpp.h>
#include <thread>

// clazy:excludeall=non-pod-global-static

namespace {

bool wait_until_no_consumers(lsl::stream_outlet &outlet, double timeout_sec) {
	auto deadline =
		std::chrono::steady_clock::now() + std::chrono::duration<double>(timeout_sec);
	while (outlet.have_consumers() && std::chrono::steady_clock::now() < deadline)
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	return !outlet.have_consumers();
}

TEST_CASE("have_consumers becomes false after idle inlet disconnects", "[outlet][basic]") {
	lsl::stream_info info("have_consumers_idle", "Markers", 1, lsl::IRREGULAR_RATE, lsl::cf_int32,
		"have_consumers_idle");
	lsl::stream_outlet outlet(info);
	auto found = lsl::resolve_stream("name", info.name(), 1, 2.0);
	REQUIRE(found.size() == 1);

	{
		lsl::stream_inlet inlet(found[0]);
		inlet.open_stream(2);
		REQUIRE(outlet.wait_for_consumers(2));
		REQUIRE(outlet.have_consumers());
		// Intentionally do not push samples; disconnect must still be detected (#267).
	}

	CHECK(wait_until_no_consumers(outlet, 2.0));
}

TEST_CASE("have_consumers becomes false after disconnect during push", "[outlet][basic]") {
	lsl::stream_info info("have_consumers_busy", "Markers", 1, lsl::IRREGULAR_RATE, lsl::cf_int32,
		"have_consumers_busy");
	lsl::stream_outlet outlet(info);
	auto found = lsl::resolve_stream("name", info.name(), 1, 2.0);
	REQUIRE(found.size() == 1);

	std::atomic<bool> keep_pushing{true};
	std::thread pusher([&outlet, &keep_pushing]() {
		int32_t value = 0;
		while (keep_pushing.load(std::memory_order_relaxed)) {
			outlet.push_sample(&value);
			++value;
		}
	});
	// Stop and join even if open_stream() throws or a REQUIRE aborts the test.
	struct stop_and_join {
		std::atomic<bool> &keep_pushing;
		std::thread &pusher;
		~stop_and_join() {
			keep_pushing.store(false, std::memory_order_relaxed);
			pusher.join();
		}
	} pusher_cleanup{keep_pushing, pusher};

	{
		lsl::stream_inlet inlet(found[0]);
		inlet.open_stream(2);
		REQUIRE(outlet.wait_for_consumers(2));
		REQUIRE(outlet.have_consumers());
		int32_t received = 0;
		CHECK(inlet.pull_sample(&received, 1, 1.0) != 0.0);
		// Destroy the inlet while the outlet thread is still producing.
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}

	CHECK(wait_until_no_consumers(outlet, 2.0));
}

TEST_CASE("have_consumers tracks explicit close and reopen", "[outlet][basic]") {
	lsl::stream_info info("have_consumers_reopen", "Markers", 1, lsl::IRREGULAR_RATE,
		lsl::cf_int32, "have_consumers_reopen");
	lsl::stream_outlet outlet(info);
	auto found = lsl::resolve_stream("name", info.name(), 1, 2.0);
	REQUIRE(found.size() == 1);

	lsl::stream_inlet inlet(found[0]);
	for (int cycle = 0; cycle < 3; ++cycle) {
		CAPTURE(cycle);
		inlet.open_stream(2);
		REQUIRE(outlet.wait_for_consumers(2));
		// close_stream() instead of destroying the inlet; the outlet must notice either way.
		inlet.close_stream();
		REQUIRE(wait_until_no_consumers(outlet, 2.0));
	}
}

TEST_CASE("have_consumers stays true when one of two inlets disconnects", "[outlet][basic]") {
	lsl::stream_info info("have_consumers_two", "Markers", 1, lsl::IRREGULAR_RATE, lsl::cf_int32,
		"have_consumers_two");
	lsl::stream_outlet outlet(info);
	auto found = lsl::resolve_stream("name", info.name(), 1, 2.0);
	REQUIRE(found.size() == 1);

	lsl::stream_inlet keeper(found[0]);
	keeper.open_stream(2);
	REQUIRE(outlet.wait_for_consumers(2));

	{
		lsl::stream_inlet leaver(found[0]);
		leaver.open_stream(2);
		REQUIRE(outlet.have_consumers());
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	// Unregistering one consumer must leave the other registered and still fed.
	CHECK(outlet.have_consumers());
	int32_t sent = 42, received = 0;
	outlet.push_sample(&sent);
	CHECK(keeper.pull_sample(&received, 1, 2.0) != 0.0);
	CHECK(received == sent);

	keeper.close_stream();
	CHECK(wait_until_no_consumers(outlet, 2.0));
}

} // namespace
