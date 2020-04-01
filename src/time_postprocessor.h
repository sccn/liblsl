#ifndef TIME_POSTPROCESSOR_H
#define TIME_POSTPROCESSOR_H

#include "common.h"
#include <functional>
#include <mutex>


namespace lsl {

/// A callback function that allows the post-processor to query state from other objects if needed
using postproc_callback_t = std::function<double()>;
using reset_callback_t = std::function<bool()>;


/// Internal class of an inlet that is responsible for post-processing time stamps.
class time_postprocessor {
public:
	/// Construct a new time post-processor given some callback functions.
	time_postprocessor(const postproc_callback_t &query_correction,
		const postproc_callback_t &query_srate, const reset_callback_t &query_reset);

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
	void set_options(uint32_t options = proc_ALL) { options_ = options; }

	/// Post-process the given time stamp and return the new time-stamp.
	double process_timestamp(double value);

	/// Override the half-time (forget factor) of the time-stamp smoothing.
	void smoothing_halftime(float value) { halftime_ = value; }

private:
	/// Internal function to process a time stamp.
	double process_internal(double value);

	/// number of samples seen so far
	double samples_seen_;

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

	// runtime parameters for smoothing
	/// first observed time-stamp value, used as a baseline to improve numerics
	double baseline_value_;
	/// linear regression model coefficients
	double w0_, w1_;
	/// inverse covariance matrix
	double P00_, P01_, P10_, P11_;
	/// forget factor lambda in RLS calculation & its inverse
	double lam_, il_;
	/// whether smoothing can be applied to these data (false if irregular srate)
	bool smoothing_applicable_;
	/// whether the smoothing parameters have been initialized already
	bool smoothing_initialized_;

	// runtime parameters for monotonize
	/// last observed time-stamp value, to force monotonically increasing stamps
	double last_value_;

	/// a mutex that protects the runtime data structures
	std::mutex processing_mut_;
};


} // namespace lsl

#endif
