#ifndef LSL_COMMON_H
#define LSL_COMMON_H

#include <stdint.h>

#ifdef LIBLSL_STATIC
#define LIBLSL_C_API
#elif defined _WIN32 || defined __CYGWIN__
#if defined LIBLSL_EXPORTS
#define LIBLSL_C_API __declspec(dllexport)
#else
#define LIBLSL_C_API __declspec(dllimport)
#ifdef _WIN64
#define LSLBITS "64"
#else
#define LSLBITS "32"
#endif
#if defined _DEBUG && defined LSL_DEBUG_BINDINGS
#define LSLLIBPOSTFIX "-debug"
#else
#define LSLLIBPOSTFIX ""
#endif
#ifndef LSLNOAUTOLINK
#pragma comment(lib, "liblsl" LSLBITS LSLLIBPOSTFIX ".lib")
#endif
#endif
#pragma warning(disable : 4275)
#else // Linux / OS X
#define LIBLSL_C_API __attribute__((visibility("default")))
#endif

/* =========================== */
/* ==== Defined constants ==== */
/* =========================== */

/**
 * Constant to indicate that a stream has variable sampling rate.
 */
#define LSL_IRREGULAR_RATE 0.0

/**
 * Constant to indicate that a sample has the next successive time stamp.
 * This is an optional optimization to transmit less data per sample.
 * The stamp is then deduced from the preceding one according to the stream's sampling rate
 * (in the case of an irregular rate, the same time stamp as before will is assumed).
 */
#define LSL_DEDUCED_TIMESTAMP -1.0

/**
 * A very large time value (ca. 1 year); can be used in timeouts.
 */
#define LSL_FOREVER 32000000.0

/**
 * Constant to indicate that there is no preference about how a data stream shall be chunked for
 * transmission. (can be used for the chunking parameters in the inlet or the outlet).
 */
#define LSL_NO_PREFERENCE 0

/**
 * Data format of a channel (each transmitted sample holds an array of channels).
 */
typedef enum {
	cft_float32 =
		1,			  /* For up to 24-bit precision measurements in the appropriate physical unit
			   (e.g.,  microvolts). Integers from -16777216 to 16777216 are represented accurately. */
	cft_double64 = 2, /* For universal numeric data as long as permitted by network & disk budget.
						 The largest representable integer is 53-bit. */
	cft_string = 3,   /* For variable-length ASCII strings or data blobs, such as video frames,
						 complex event descriptions, etc. */
	cft_int32 = 4,	/* For high-rate digitized formats that require 32-bit precision. Depends
							 critically on meta-data to represent meaningful units. Useful for
						   application    event codes or other coded data. */
	cft_int16 = 5, /* For very high rate signals (40Khz+) or consumer-grade audio (for professional
					  audio float is recommended). */
	cft_int8 =
		6, /* For binary signals or other coded data. Not recommended for encoding string data. */
	cft_int64 = 7,	/* For now only for future compatibility. Support for this type is not yet
							 exposed in all languages. Also, some builds of liblsl will not be able to
						   send or receive data of this type. */
	cft_undefined = 0 /* Can not be transmitted. */
} lsl_channel_format_t;

/**
 * Post-processing options for stream inlets.
 */
typedef enum {
	proc_none = 0, /* No automatic post-processing; return the ground-truth time stamps for manual
					  post-processing
				   (this is the default behavior of the inlet). */
	proc_clocksync = 1,  /* Perform automatic clock synchronization; equivalent to manually adding
							the time_correction() value to the received time stamps. */
	proc_dejitter = 2,   /* Remove jitter from time stamps. This will apply a smoothing algorithm to
							the received time stamps; the smoothing needs to see a minimum number of
		samples (30-120 seconds worst-case) until the remaining  jitter is consistently below 1ms. */
	proc_monotonize = 4, /* Force the time-stamps to be monotonically ascending (only makes sense if
							timestamps are dejittered). */
	proc_threadsafe = 8, /* Post-processing is thread-safe (same inlet can be read from by multiple
							threads); uses somewhat more CPU. */
	proc_ALL = 1 | 2 | 4 | 8 /* The combination of all possible post-processing options. */
} lsl_processing_options_t;

/**
 * Possible error codes.
 */
typedef enum {
	lsl_no_error = 0,		/* No error occurred */
	lsl_timeout_error = -1, /* The operation failed due to a timeout. */
	lsl_lost_error = -2,	/* The stream has been lost. */
	lsl_argument_error =
		-3, /* An argument was incorrectly specified (e.g., wrong format or wrong length). */
	lsl_internal_error = -4 /* Some other internal error has happened. */
} lsl_error_code_t;

#endif
