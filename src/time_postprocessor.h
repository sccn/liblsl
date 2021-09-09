#ifndef TIME_POSTPROCESSOR_H
#define TIME_POSTPROCESSOR_H

#include "common.h"
#include <cstdint>
#include <functional>
#include <mutex>

namespace lsl {

/// A callback function that allows the post-processor to query state from other objects if needed
using postproc_callback_t = std::function<double()>;
using reset_callback_t = std::function<bool()>;

/// Dejitter / smooth timestamps with a first order recursive least squares filter (RLS).
struct postproc_dejitterer {
	/// first observed time-stamp value, used as a baseline to improve numerics
	uint_fast32_t t0_;
	/// number of samples since t0, i.e. the x in y=ax+b
	uint_fast32_t samples_since_t0_{0};
	/// linear regression model coefficients (intercept, slope)
	double w0_{0}, w1_{0};
	/// inverse covariance matrix elements (P00, P11, off diagonal element P01=P10)
	double P00_{1e10}, P11_{1e10}, P01_{0};
	/// forget factor lambda in RLS calculation
	double lam_{0};

	/// constructor
	postproc_dejitterer(double t0 = 0, double srate = 0, double halftime = 0);

	/// dejitter a timestamp and update RLS parameters
	double dejitter(double t) noexcept;

	/// adjust RLS parameters to account for samples not seen
	void skip_samples(uint_fast32_t skipped_samples) noexcept;
	bool is_initialized() const noexcept { return t0_ != 0; }
	bool smoothing_applicable() const noexcept { return lam_ > 0; }
};

/// Internal class of an inlet that is responsible for post-processing time stamps.
class time_postprocessor {
public:
	/// Construct a new time post-processor given some callback functions.
	time_postprocessor(postproc_callback_t query_correction, postproc_callback_t query_srate,
		reset_callback_t query_reset);

	/**
	 * Set post-processing options to use.
	 *
	 * By default, this class performs NO post-processing and returns the ground-truth time stamps,
	 * which can then be manually synchronized using time_correction(), and then smoothed/dejittered
	 * if desired. This function allows automating these two and possibly more operations.
	 * @param flags An integer that is the result of bitwise OR'ing one or more options from
	 * processing_options_t together (e.g., proc_clocksync|proc_dejitter); the default is to enable
	 * all options.
	 */
	void set_options(uint32_t options = proc_ALL);

	/// Post-process the given time stamp and return the new time-stamp.
	double process_timestamp(double value);

	/// Override the half-time (forget factor) of the time-stamp smoothing.
	void smoothing_halftime(float value) { halftime_ = value; }

	/// Inform the post processor some samples were skipped
	void skip_samples(uint32_t skipped_samples);

private:
	/// Internal function to process a time stamp.
	double process_internal(double value);

	/// number of samples seen since last clocksync
	uint8_t samples_since_last_clocksync;

	// configuration parameters
	/// a callback function that returns the current nominal sampling rate
	postproc_callback_t query_srate_;
	/// current processing options
	uint32_t options_;
	/// smoothing half-time
	float halftime_;

	// handling of time corrections
	/// a callback function that returns the current time-correction offset
	postproc_callback_t query_correction_;
	/// a callback function that returns whether the clock was reset
	reset_callback_t query_reset_;
	/// the next time when we query the time-correction offset
	double next_query_time_;
	/// last queried correction offset
	double last_offset_;

	postproc_dejitterer dejitter;

	// runtime parameters for monotonize
	/// last observed time-stamp value, to force monotonically increasing stamps
	double last_value_;

	/// a mutex that protects the runtime data structures
	std::mutex processing_mut_;
};


} // namespace lsl

#endif
