#include <catch2/catch_all.hpp>
#include <lsl_cpp.h>
#include <atomic>
#include <chrono>
#include <thread>

namespace {
// Repeat the sample so these lifecycle tests do not depend on the separate
// open_stream()/outlet-readiness race (#176), or stale outlet consumer counts.
class sample_sender {
public:
	sample_sender(lsl::stream_outlet &outlet, float value)
		: thread_([this, &outlet, value]() {
			while (!stop_) {
				outlet.push_sample(&value);
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
		}) {}
	~sample_sender() {
		stop_ = true;
		thread_.join();
	}
private:
	std::atomic<bool> stop_{false};
	std::thread thread_;
};
}

TEST_CASE("An inlet can close and reopen repeatedly", "[inlet][reopen]") {
	const bool recover = GENERATE(false, true);
	const bool explicit_open = GENERATE(false, true);
	CAPTURE(recover, explicit_open);
	lsl::stream_info info("reopen", "test", 1, 0, lsl::cf_float32, "reopen-source-" + std::to_string(lsl::local_clock()));
	lsl::stream_outlet outlet(info);
	auto found = lsl::resolve_stream("source_id", info.source_id(), 1, 2);
	REQUIRE(found.size() == 1);
	lsl::stream_inlet inlet(found[0], 360, 0, recover);

	// Closing an inlet before its first open, or closing it twice, is harmless.
	inlet.close_stream();
	inlet.close_stream();
	for (int cycle = 1; cycle <= 3; ++cycle) {
		if (explicit_open) inlet.open_stream(2);
		{
			sample_sender sender(outlet, static_cast<float>(cycle));
			float received = 0;
			REQUIRE(inlet.pull_sample(&received, 1, 2) != 0);
			CHECK(received == static_cast<float>(cycle));
			// Leave unread data in the queue to check the documented close semantics.
			const double deadline = lsl::local_clock() + 2;
			while (!inlet.samples_available() && lsl::local_clock() < deadline)
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			REQUIRE(inlet.samples_available() > 0);
		}
		inlet.close_stream();
		CHECK(inlet.samples_available() == 0);
		inlet.close_stream();
	}
}

TEST_CASE("Closing an inlet interrupts recovery and permits a later open", "[inlet][reopen]") {
	const bool request_info_first = GENERATE(false, true);
	CAPTURE(request_info_first);
	lsl::stream_info info("reopen-missing", "test", 1, 0, lsl::cf_float32,
		"reopen-missing-source-" + std::to_string(lsl::local_clock()));
	lsl::stream_inlet inlet(info);
	// The metadata receiver can own the recovery lock while the data receiver
	// waits for it. Closing data must leave the metadata receiver operational.
	if (request_info_first) REQUIRE_THROWS_AS(inlet.info(0.1), lsl::timeout_error);
	REQUIRE_THROWS_AS(inlet.open_stream(0.1), lsl::timeout_error);
	const double start = lsl::local_clock();
	inlet.close_stream();
	CHECK(lsl::local_clock() - start < 2);
	CHECK(inlet.samples_available() == 0);

	lsl::stream_outlet outlet(info);
	inlet.open_stream(5);
	{
		sample_sender sender(outlet, 42);
		float received = 0;
		REQUIRE(inlet.pull_sample(&received, 1, 2) != 0);
		CHECK(received == 42);
	}
	CHECK(inlet.info(2).source_id() == info.source_id());
	inlet.close_stream();
}

TEST_CASE("Closing during connection setup permits reopening", "[inlet][reopen]") {
	lsl::stream_info info("reopen-setup", "test", 1, 0, lsl::cf_float32, "reopen-setup-source-" + std::to_string(lsl::local_clock()));
	lsl::stream_outlet outlet(info);
	auto found = lsl::resolve_stream("source_id", info.source_id(), 1, 2);
	REQUIRE(found.size() == 1);
	lsl::stream_inlet inlet(found[0], 360, 0, false);
	for (int cycle = 0; cycle < 20; ++cycle) {
		try {
			inlet.open_stream(0);
		} catch (const lsl::timeout_error &) {
			// The receiver may not have registered its cancellable socket yet.
		}
		inlet.close_stream();
	}
	inlet.open_stream(2);
	sample_sender sender(outlet, 42);
	float received = 0;
	REQUIRE(inlet.pull_sample(&received, 1, 2) != 0);
	CHECK(received == 42);
}
