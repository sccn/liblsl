#include "../common/create_streampair.hpp"
#include <atomic>
#include <catch2/catch.hpp>
#include <lsl_cpp.h>
#include <thread>

// clazy:excludeall=non-pod-global-static

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


TEST_CASE("timeouts", "[pull][basic]") {
	auto sp = create_streampair(
		lsl::stream_info("timeouts", "Test", 1, lsl::IRREGULAR_RATE, lsl::cf_int8, "timeouts"));
	std::atomic<bool> done{false};

	// Push a sample after some time so the test can continue even if the timeout isn't honored
	std::thread saver([&]() {
		char val;
		auto end = lsl::local_clock() + 2;
		while (!done && lsl::local_clock() < end)
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		sp.out_.push_sample(&val);
	});

	char val;
	REQUIRE(sp.in_.pull_sample(&val, 1, 0.5) == Approx(0.0));
	done = true;
	saver.join();
}
} // namespace
