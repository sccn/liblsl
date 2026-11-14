#include "time_postprocessor.h"
#include <loguru.hpp>
#include <random>
#include <thread>
// include loguru before catch
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// clazy:excludeall=non-pod-global-static

template <std::size_t N>
inline void test_postproc_array(
	lsl::time_postprocessor &pp, const double (&in)[N], const double (&expected)[N]) {
	for (std::size_t i = 0; i < N; ++i) CHECK(pp.process_timestamp(in[i]) == Catch::Approx(expected[i]));
}

TEST_CASE("postprocessing", "[basic]") {
	double time_offset = -50.0, srate = 1.;
	bool was_reset = false;
	lsl::time_postprocessor pp(
		[&]() {
			UNSCOPED_INFO("time_offset " << time_offset);
			return time_offset;
		},
		[&]() { return srate; }, [&]() { return was_reset; });

	double nopostproc[] = {2, 3.1, 3, 5, 5.9, 7.1};

	{
		INFO("proc_clocksync");
		pp.set_options(proc_clocksync);
		for (double t : nopostproc) CHECK(pp.process_timestamp(t) == Catch::Approx(t + time_offset));
	}

	{
		INFO("proc_clocksync + clock_reset");
		pp.set_options(proc_clocksync);
		// std::this_thread::sleep_for(std::chrono::milliseconds(600));
		was_reset = true;
		time_offset = -200;
		// for (double t : nopostproc) CHECK(pp.process_timestamp(t) == Approx(t + time_offset));
	}

	{
		INFO("proc_monotonize");
		double monotonized[] = {2, 3.1, 3.1, 5, 5.9, 7.1};
		pp.set_options(proc_monotonize);
		test_postproc_array(pp, nopostproc, monotonized);
	}

	{
		INFO("reset with proc_none");
		pp.set_options(proc_none);
		test_postproc_array(pp, nopostproc, nopostproc);
	}
}

TEST_CASE("rls_smoothing", "[basic]") {
	const int n = 100000, warmup_samples = 1000;
	const double t0 = 5000, latency = 0.05, srate = 100., halftime = 90;
	lsl::postproc_dejitterer pp(t0, srate, halftime);

	std::default_random_engine rng;
	std::normal_distribution<double> jitter(latency, .005);

	pp.dejitter(t0);
	int n_outlier = 0;

	for (int i = 0; i < n; ++i) {
		const double t = t0 + i / srate, e = jitter(rng), dejittered = pp.dejitter(t + e),
					 est_error = dejittered - t - latency;
		if (i > warmup_samples && fabs(est_error) > std::max(e, .001)) n_outlier++;
	}
	LOG_F(INFO, "\nP:\t%f\t%f\n\t%f\t%f\nw:\t%f\t%f", pp.P00_, pp.P01_, pp.P01_, pp.P11_, pp.w0_,
		pp.w1_);
	CHECK(n_outlier < n / 100);

	CHECK(pp.dejitter(t0 + latency + n / srate) == Catch::Approx(t0 + latency + n / srate));
	CHECK(fabs(pp.w0_ - latency) < .1);
	CHECK(fabs(pp.w1_ - 1 / srate) < 1e-6);
}

TEST_CASE("rls_smoothing_break", "[basic]") {
	const double t0 = 5000, srate = 100., halftime = 90, pause = 2;
	for (double break_seconds : {0., 1.}) {
		INFO("break threshold " << break_seconds);
		const double break_samples = break_seconds > 0 ? 50 : 0;
		lsl::postproc_dejitterer pp(t0, srate, halftime, break_seconds, break_samples);
		std::default_random_engine rng;
		std::normal_distribution<double> jitter(0, .002);
		double max_err_after = 0;
		for (int i = 0; i < 400 * srate; ++i) {
			// 300 s of data, then the sender pauses for 2 s
			const double t = t0 + i / srate + (i >= 300 * srate ? pause : 0), tj = t + jitter(rng);
			// as in time_postprocessor::process_internal
			if (pp.is_break(tj))
				pp = lsl::postproc_dejitterer(tj, srate, halftime, break_seconds, break_samples);
			const double err = pp.dejitter(tj) - t;
			if (i >= 300 * srate) max_err_after = std::max(max_err_after, fabs(err));
		}
		if (break_seconds > 0)
			CHECK(max_err_after < .02);
		else
			CHECK(max_err_after > 1.5); // today's behaviour: the break is smoothed over
	}
}
