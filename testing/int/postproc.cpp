#include "time_postprocessor.h"
#include <loguru.hpp>
#include <random>
#include <thread>
// include loguru before catch
#include <catch2/catch_approx.cpp>
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
