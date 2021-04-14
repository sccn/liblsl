#include "time_postprocessor.h"
#include <catch2/catch.hpp>
#include <thread>

template <std::size_t N>
inline void test_postproc_array(
	lsl::time_postprocessor &pp, const double (&in)[N], const double (&expected)[N]) {
	for (std::size_t i = 0; i < N; ++i) CHECK(pp.process_timestamp(in[i]) == Approx(expected[i]));
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
		INFO("proc_clocksync")
		pp.set_options(proc_clocksync);
		for (double t : nopostproc) CHECK(pp.process_timestamp(t) == Approx(t + time_offset));
	}

	{
		INFO("proc_clocksync + clock_reset")
		pp.set_options(proc_clocksync);
		// std::this_thread::sleep_for(std::chrono::milliseconds(600));
		was_reset = true;
		time_offset = -200;
		// for (double t : nopostproc) CHECK(pp.process_timestamp(t) == Approx(t + time_offset));
	}

	{
		INFO("proc_monotonize")
		double monotonized[] = {2, 3.1, 3.1, 5, 5.9, 7.1};
		pp.set_options(proc_monotonize);
		test_postproc_array(pp, nopostproc, monotonized);
	}

	{
		INFO("reset with proc_none")
		pp.set_options(proc_none);
		test_postproc_array(pp, nopostproc, nopostproc);
	}
	{
		INFO("proc_dejitter")
		pp.set_options(proc_dejitter);
		/// ground truth timestamps as dejittered by liblsl 1.14
		double dejittered[] = {2.00405, 3.1, 3.1978, 4.6113, 5.7418, 6.9168};
		test_postproc_array(pp, nopostproc, dejittered);
	}
}
