#pragma once

/** @file constants.h
 * @brief Global constants for liblsl */

//! Constant to indicate that a stream has variable sampling rate.
#define LSL_IRREGULAR_RATE 0.0

/**
 * Constant to indicate that a sample has the next successive time stamp.
 * This is an optional optimization to transmit less data per sample.
 * The stamp is then deduced from the preceding one according to the stream's sampling rate
 * (in the case of an irregular rate, the same time stamp as before will is assumed).
 */
#define LSL_DEDUCED_TIMESTAMP -1.0

//! A very large time value (ca. 1 year); can be used in timeouts.
#define LSL_FOREVER 32000000.0

/**
 * Constant to indicate that there is no preference about how a data stream shall be chunked for
 * transmission. (can be used for the chunking parameters in the inlet or the outlet).
 */
#define LSL_NO_PREFERENCE 0

//! Data format of a channel (each transmitted sample holds an array of channels).
typedef enum {
	/*! For up to 24-bit precision measurements in the appropriate physical unit (e.g., microvolts).
	 * Integers from -16777216 to 16777216 are represented accurately. */
	cft_float32 = 1,

	/*! For universal numeric data as long as permitted by network & disk budget.
	 * The largest representable integer is 53-bit. */
	cft_double64 = 2,

	/*! For variable-length ASCII strings or data blobs, such as video frames, complex event
	   descriptions, etc. */
	cft_string = 3,

	/*! For high-rate digitized formats that require 32-bit precision.
	 * Depends critically on meta-data to represent meaningful units.
	 * Useful for application event codes or other coded data. */
	cft_int32 = 4,

	/*! For very high rate signals (40Khz+) or consumer-grade audio.
	 * For professional audio float is recommended. */
	cft_int16 = 5,

	/*! For binary signals or other coded data. Not recommended for encoding string data. */
	cft_int8 = 6,

	/*! For now only for future compatibility. Support for this type is not yet
							 exposed in all languages. Also, some builds of liblsl will not be able
	   to send or receive data of this type.*/
	cft_int64 = 7,

	//! Can not be transmitted.
	cft_undefined = 0
} lsl_channel_format_t;

//! Post-processing options for stream inlets.
typedef enum {
	/*! No automatic post-processing; return the ground-truth time stamps for manual
	 * post-processing. This is the default behavior of the inlet. */
	proc_none = 0,

	/*! Perform automatic clock synchronization;
	 * equivalent to manually adding the time_correction() value to the received time stamps. */
	proc_clocksync = 1,

	/*! Remove jitter from time stamps.<br>
	 * This will apply a smoothing algorithm to the received time stamps;
	 * the smoothing needs to see a minimum number of samples (30-120 seconds worst-case)
	 * until the remaining jitter is consistently below 1ms. */
	proc_dejitter = 2,

	/*! Force the time-stamps to be monotonically ascending.<br>
	 * Only makes sense if timestamps are dejittered. */
	proc_monotonize = 4,

	/*! Post-processing is thread-safe (same inlet can be read from by multiple threads);
	 * uses somewhat more CPU. */
	proc_threadsafe = 8,

	//! The combination of all possible post-processing options.
	proc_ALL = 1 | 2 | 4 | 8
} lsl_processing_options_t;

/**
 * Possible error codes.
 */
typedef enum {
	//! No error occurred
	lsl_no_error = 0,

	//! The operation failed due to a timeout.
	lsl_timeout_error = -1,

	//! The stream has been lost.
	lsl_lost_error = -2,

	//! An argument was incorrectly specified (e.g., wrong format or wrong length).
	lsl_argument_error = -3,

	//! Some other internal error has happened.
	lsl_internal_error = -4
} lsl_error_code_t;
