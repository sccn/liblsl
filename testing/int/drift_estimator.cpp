#include "drift_estimator.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <random>

using lsl::drift_estimator;

namespace {
/// feed rounds every 2 s: offset = 1 ms + ppm * t + gaussian noise (sd 15 us), rtt 300 us
void feed(drift_estimator &e, double ppm, double t_end, std::mt19937 &rng, double t0 = 0) {
	std::normal_distribution<double> noise(0, 15e-6);
	for (double t = t0; t <= t_end; t += 2) e.add(t, 1e-3 + ppm * 1e-6 * t + noise(rng), 300e-6);
}
} // namespace

TEST_CASE("drift estimator tracks pure drift", "[timesync][drift]") {
	for (double ppm : {20.0, -20.0}) {
		std::mt19937 rng(42);
		drift_estimator e(60);
		feed(e, ppm, 120, rng);
		REQUIRE(e.fitted());
		CHECK(e.slope() == Catch::Approx(ppm * 1e-6).margin(2e-6));
		// extrapolated 1.5 s past the last round; holding the last round would be off by
		// noise + 30 us of drift
		const double t = 121.5;
		CHECK(std::abs(e.predict(t) - (1e-3 + ppm * 1e-6 * t)) < 15e-6);
	}
}

TEST_CASE("drift estimator ignores an outlier round", "[timesync][drift]") {
	std::mt19937 rng(1);
	drift_estimator e(60);
	feed(e, 20, 100, rng);
	const double before = e.predict(103);
	e.add(102, 1e-3 + 20e-6 * 102 + 3.5e-3, 7e-3); // late reply: +3.5 ms, rtt 7 ms
	CHECK(std::abs(e.predict(103) - before) < 10e-6);
	CHECK(e.fitted());
}

TEST_CASE("drift estimator falls back to the latest round", "[timesync][drift]") {
	drift_estimator e(60);
	for (int k = 0; k < 4; k++) e.add(2.0 * k, 1e-3 + k * 1e-5, 300e-6); // < kMinRounds
	CHECK(!e.fitted());
	CHECK(e.predict(100) == 1e-3 + 3 * 1e-5);
	e.add(8, 1e-3 + 4e-5, 300e-6);
	CHECK(e.fitted());
	CHECK(e.slope() == Catch::Approx(5e-6));
	// rounds closer together than kMinSpan are not enough either
	drift_estimator f(60);
	for (int k = 0; k < 10; k++) f.add(0.5 * k, 2e-3, 300e-6);
	CHECK(!f.fitted());
}

TEST_CASE("drift estimator distrusts absurd slopes", "[timesync][drift]") {
	drift_estimator e(60);
	for (int k = 0; k < 10; k++) e.add(2.0 * k, 2e-3 * k, 300e-6); // 1000 ppm
	CHECK(!e.fitted());
	CHECK(e.predict(20) == 2e-3 * 9);
}

TEST_CASE("drift estimator same clock gives exactly 0", "[timesync][drift]") {
	drift_estimator e(30);
	for (int k = 0; k < 20; k++) e.add(2.0 * k, 0.0, 0.0);
	REQUIRE(e.fitted());
	CHECK(e.predict(41.3) == 0.0);
	CHECK(!std::signbit(e.predict(41.3)));
}

TEST_CASE("drift estimator follows a rate change", "[timesync][drift]") {
	std::mt19937 rng(7);
	drift_estimator e(60);
	feed(e, 10, 100, rng);
	// the clock rate changes by +50 ppm; a 60 s median line alone would ignore that for ~30 s
	std::normal_distribution<double> noise(0, 15e-6);
	double worst = 0;
	for (double t = 102; t <= 140; t += 2) {
		const double truth = 1e-3 + 10e-6 * t + 50e-6 * (t - 100);
		worst = std::max(worst, std::abs(e.predict(t) - truth)); // before this round arrives
		e.add(t, truth + noise(rng), 300e-6);
	}
	CHECK(worst < 400e-6); // a 60 s median line alone would reach ~1.5 ms here
	const double t = 141;
	CHECK(std::abs(e.predict(t) - (1e-3 + 10e-6 * t + 50e-6 * (t - 100))) < 30e-6);
}

TEST_CASE("drift estimator ignores a short excursion", "[timesync][drift]") {
	std::mt19937 rng(11);
	drift_estimator e(60);
	feed(e, 10, 100, rng);
	const double before = e.predict(107);
	// two rounds over a different network path, +55 us with a smaller rtt (seen macOS<->Linux)
	e.add(102, 1e-3 + 10e-6 * 102 + 55e-6, 250e-6);
	e.add(104, 1e-3 + 10e-6 * 104 + 55e-6, 250e-6);
	e.add(106, 1e-3 + 10e-6 * 106, 300e-6);
	CHECK(std::abs(e.predict(107) - before) < 10e-6);
	CHECK(e.fitted()); // no reset: the history was kept
}

TEST_CASE("drift estimator clear forgets the history", "[timesync][drift]") {
	std::mt19937 rng(3);
	drift_estimator e(60);
	feed(e, 20, 60, rng);
	REQUIRE(e.fitted());
	e.clear();
	CHECK(e.empty());
	CHECK(!e.fitted());
	e.add(1000, 5e-3, 300e-6); // e.g. a different host after recovery
	CHECK(e.predict(1001) == 5e-3);
}
