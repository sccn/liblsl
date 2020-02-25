#include "api_config.h"
#include "common.h"
#include <algorithm>
#include <boost/chrono/duration.hpp>
#include <boost/chrono/system_clocks.hpp>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment (lib,"winmm.lib")
#endif

// === Implementation of the free-standing functions in lsl_c.h ===

extern "C" {
/// Get the protocol version.
LIBLSL_C_API int32_t lsl_protocol_version() {
	return lsl::api_config::get_instance()->use_protocol_version();
}

/// Get the library version.
LIBLSL_C_API int32_t lsl_library_version() { return LSL_LIBRARY_VERSION; }

/// Get a string containing library information
LIBLSL_C_API const char *lsl_library_info() {
#ifdef LSL_LIBRARY_INFO_STR
	return LSL_LIBRARY_INFO_STR;
#else
	return "Unknown (not set by build system)";
#endif
}

/** Obtain a local system time stamp in seconds.
 *
 * The resolution is better than a millisecond.
 * This reading can be used to assign time stamps to samples as they are being
 * acquired.
 *
 * If the "age" of a sample is known at a particular time (e.g., from USB
 * transmission delays), it can be used as an offset to local_clock() to obtain
 * a better estimate of when a sample was actually captured. */
LIBLSL_C_API double lsl_local_clock() {
	return lslboost::chrono::nanoseconds(
			   lslboost::chrono::high_resolution_clock::now().time_since_epoch())
			   .count() /
		   1000000000.0;
}


/** Deallocate a string that has been transferred to the application.
 *
 * The only use case is to deallocate the contents of string-valued samples
 * received from LSL in an application where no free() method is available
 * (e.g., in some scripting languages). */
LIBLSL_C_API void lsl_destroy_string(char *s) {
	if (s) free(s);
}
}

// === implementation of misc functions ===
/// Implementation of the clock facility.
double lsl::lsl_clock() { 
	return lsl_local_clock();
}

/// Ensure that LSL is initialized. Performs initialization tasks
void lsl::ensure_lsl_initialized() {
	static bool is_initialized = false;

	if (!is_initialized) {
		is_initialized = true;

		loguru::g_stderr_verbosity = loguru::Verbosity_WARNING;
#ifdef LOGURU_DEBUG_LOGGING
		// Initialize loguru, mainly to print stacktraces on segmentation faults
		int argc = 1;
		const char *argv[] = {"liblsl", nullptr};
		loguru::init(argc, const_cast<char **>(argv));
#else
#endif

#ifdef _WIN32
		// if a timer resolution other than 0 is requested (0 means don't override)...
		if (int desired_timer_resolution = lsl::api_config::get_instance()->timer_resolution()) {			
			// then override it for the lifetime of this program
			struct override_timer_resolution_until_exit {
				override_timer_resolution_until_exit(int res): res_(res) { timeBeginPeriod(res_); }
				~override_timer_resolution_until_exit() { timeEndPeriod(res_); }
				int res_;
			};
			static override_timer_resolution_until_exit overrider(desired_timer_resolution);
		}
#endif
	}
}

std::vector<std::string> lsl::splitandtrim(
	const std::string &input, char separator, bool keepempty) {
	std::vector<std::string> parts;
	auto it = input.cbegin();
	while(true) {
		// Skip whitespace in the beginning
		while (it != input.cend() && std::isspace(*it)) ++it;

		// find the next separator or end of string
		auto endit = std::find(it, input.cend(), separator);
		// mark beginning of next part if not at the end
		auto next = endit;

		// shrink the range so it doesn't include whitespace at the end
		while (it < endit && std::isspace(*(endit - 1))) --endit;
		if (endit != it || keepempty) parts.emplace_back(it, endit);

		if (next != input.cend()) it = next + 1;
		else break;
	}
	return parts;
}

std::string lsl::trim(const std::string& input)
{
	auto first = input.find_first_not_of(" \t\r\n"), last = input.find_last_not_of(" \t\r\n");
	if(first == std::string::npos || last == std::string::npos) return "";
	return input.substr(first, last-first+1);
}
