#include "time_receiver.h"
#include <catch2/catch_test_macros.hpp>

// clazy:excludeall=non-pod-global-static

using lsl::same_clock_fallback;

static bool round(same_clock_fallback &f, bool token, double t0, double t1, double t2, double t3) {
	f.start_round();
	f.add_probe(token, t0, t1, t2, t3);
	f.add_probe(token, t0 + 1, t1 + 1, t2 + 1, t3 + 1);
	return f.end_round();
}

TEST_CASE("same clock fallback", "[timesync][basic]") {
	CHECK(lsl::zero_offset_consistent(1, 1.0001, 1.0002, 1.0003));
	CHECK_FALSE(lsl::zero_offset_consistent(1, 0.9999, 1.0002, 1.0003)); // outlet clock behind
	CHECK_FALSE(lsl::zero_offset_consistent(1, 1.0001, 1.0004, 1.0003)); // outlet clock ahead

	same_clock_fallback f(3);
	// snaps after 3 consistent rounds in a row
	CHECK_FALSE(round(f, false, 10, 10.1, 10.2, 10.3));
	CHECK_FALSE(round(f, false, 10, 10.1, 10.2, 10.3));
	CHECK(round(f, false, 10, 10.1, 10.2, 10.3));
	CHECK(round(f, false, 10, 10.1, 10.2, 10.3));
	// one violating probe resets the count
	CHECK_FALSE(round(f, false, 10, 9.9, 10.2, 10.3));
	CHECK_FALSE(round(f, false, 10, 10.1, 10.2, 10.3));
	CHECK_FALSE(round(f, false, 10, 10.1, 10.2, 10.3));
	CHECK(round(f, false, 10, 10.1, 10.2, 10.3));
	// a round without replies resets it, too
	f.start_round();
	CHECK_FALSE(f.end_round());

	// a reply with a clock identity token is handled elsewhere and never counts
	same_clock_fallback g(1);
	CHECK_FALSE(round(g, true, 10, 10.1, 10.2, 10.3));
	CHECK(round(g, false, 10, 10.1, 10.2, 10.3));

	// distinct boot-relative clocks differ by seconds, so they never count
	same_clock_fallback h(1);
	CHECK_FALSE(round(h, false, 10, 5010.1, 5010.2, 10.3));

	// rounds = 0 disables it
	same_clock_fallback off;
	for (int i = 0; i < 5; i++) CHECK_FALSE(round(off, false, 10, 10.1, 10.2, 10.3));
}
