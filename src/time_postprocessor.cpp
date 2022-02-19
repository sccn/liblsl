#include "time_postprocessor.h"
#include "api_config.h"
#include <cmath>
#include <limits>
#include <utility>

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#elif defined(__clang__)
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"
#endif

// === implementation of the time_postprocessor class ===

using namespace lsl;

/// how many samples have to be seen between clocksyncs?
const uint8_t samples_between_clocksyncs = 50;

time_postprocessor::time_postprocessor(postproc_callback_t query_correction,
	postproc_callback_t query_srate, reset_callback_t query_reset)
	: samples_since_last_clocksync(samples_between_clocksyncs),
	  query_srate_(std::move(query_srate)), options_(proc_none),
	  halftime_(api_config::get_instance()->smoothing_halftime()),
	  query_correction_(std::move(query_correction)), query_reset_(std::move(query_reset)),
	  next_query_time_(0.0), last_offset_(0.0), last_value_(std::numeric_limits<double>::lowest()) {
}

void time_postprocessor::set_options(uint32_t options)
{
	// bitmask which options actually changed (XOR)
	auto changed = options_ ^ options;

	// dejitter option changed? -> Reset it
	// in case it got enabled, it'll be initialized with the correct t0 when
	// the next sample comes in
	if(changed & proc_dejitter)
		dejitter = postproc_dejitterer();

	if(changed & proc_monotonize)
		last_value_ = std::numeric_limits<double>::lowest();

	options_ = options;
}

double time_postprocessor::process_timestamp(double value) {
	if (options_ & proc_threadsafe) {
		std::lock_guard<std::mutex> lock(processing_mut_);
		return process_internal(value);
	}
	return process_internal(value);
}

void time_postprocessor::skip_samples(uint32_t skipped_samples) {
	if (options_ & proc_dejitter && dejitter.smoothing_applicable())
		dejitter.samples_since_t0_ += skipped_samples;
}

double time_postprocessor::process_internal(double value) {
	// --- clock synchronization ---
	if (options_ & proc_clocksync) {
		// update last correction value if needed (we do this every 50 samples and at most twice per
		// second)
		if (++samples_since_last_clocksync > samples_between_clocksyncs &&
			lsl_clock() > next_query_time_) {
			last_offset_ = query_correction_();
			samples_since_last_clocksync = 0;
			if (query_reset_()) {
				// reset state to unitialized
				last_offset_ = query_correction_();
				last_value_ = std::numeric_limits<double>::lowest();
				// reset the dejitterer to an uninitialized state so it's
				// initialized on the next use
				dejitter = postproc_dejitterer();
			}
			next_query_time_ = lsl_clock() + 0.5;
		}
		// perform clock synchronization; this is done by adding the last-measured clock offset
		// value (typically this is used to map the value from the sender's clock to our local
		// clock)
		value += last_offset_;
	}

	// --- jitter removal ---
	if (options_ & proc_dejitter) {
		// initialize the smoothing state if not yet done so
		if (!dejitter.is_initialized()) {
			double srate = query_srate_();
			dejitter = postproc_dejitterer(value, srate, halftime_);
		}
		value = dejitter.dejitter(value);
	}

	// --- force monotonic timestamps ---
	if (options_ & proc_monotonize) {
		if (value < last_value_) value = last_value_;
		else
			last_value_ = value;
	}

	return value;
}

postproc_dejitterer::postproc_dejitterer(double t0, double srate, double halftime)
	: t0_(static_cast<uint_fast32_t>(t0)) {
	if (srate > 0) {
		w1_ = 1. / srate;
		lam_ = pow(2, -1 / (srate * halftime));
	}
}

double postproc_dejitterer::dejitter(double t) noexcept {
	if (!smoothing_applicable()) return t;

	// remove baseline for better numerical accuracy
	t -= t0_;

	// RLS update
	const double u1 = samples_since_t0_++,	 // u = np.matrix([[1.0], [samples_seen]])
		pi0 = P00_ + u1 * P01_,				 // pi = u.T * P
		pi1 = P01_ + u1 * P11_,				 // ..
		al = t - (w0_ + u1 * w1_),			 // α = t - w.T * u	# prediction error
		g_inv = 1 / (lam_ + pi0 + pi1 * u1), // g_inv = 1/(lam_ + pi * u)
		il_ = 1 / lam_;						 // ...
	P00_ = il_ * (P00_ - pi0 * pi0 * g_inv); // P = (P - k*pi) / lam_
	P01_ = il_ * (P01_ - pi0 * pi1 * g_inv); // ...
	P11_ = il_ * (P11_ - pi1 * pi1 * g_inv); // ...
	w0_ += al * (P00_ + P01_ * u1);			 // w += k*α
	w1_ += al * (P01_ + P11_ * u1);			 // ...
	return w0_ + u1 * w1_ + t0_;			 // t = float(w.T * u) + t0
}

void postproc_dejitterer::skip_samples(uint_fast32_t skipped_samples) noexcept {
	samples_since_t0_ += skipped_samples;
}
