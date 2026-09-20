#include <catch2/catch_all.hpp>
#include <lsl_cpp.h>
#include <string>

TEST_CASE("The first sample after open_stream is delivered", "[inlet][open][basic]") {
	// Cover the original string-stream report, ordinary numeric outlets, and
	// synchronous numeric outlets. Each iteration uses a newly created inlet.
	const int mode = GENERATE(0, 1, 2);
	const bool strings = mode == 2;
	const bool synchronous = mode == 1;
	CAPTURE(strings, synchronous);
	const auto source_id = "open-ready-" + std::to_string(lsl::local_clock());
	lsl::stream_info info("open-ready", "test", 1, 0,
		strings ? lsl::cf_string : lsl::cf_float32, source_id);
	lsl::stream_outlet outlet(info, 0, 360,
		synchronous ? transp_sync_blocking : transp_default);
	auto found = lsl::resolve_stream("source_id", source_id, 1, 2);
	REQUIRE(found.size() == 1);

	for (int trial = 0; trial < 30; ++trial) {
		CAPTURE(trial);
		lsl::stream_inlet inlet(found[0]);
		const float sent_number = static_cast<float>(trial);
		const std::string sent_string = std::to_string(trial);
		inlet.open_stream(2);
		// No sleep, wait_for_consumers(), retry, or extra push between these calls.
		if (strings) {
			outlet.push_sample(&sent_string);
			std::string received;
			REQUIRE(inlet.pull_sample(&received, 1, 2) != 0);
			CHECK(received == sent_string);
		} else {
			outlet.push_sample(&sent_number);
			float received = -1;
			REQUIRE(inlet.pull_sample(&received, 1, 2) != 0);
			CHECK(received == sent_number);
		}
	}
}
