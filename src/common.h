#ifndef COMMON_H
#define COMMON_H

#include "../include/lsl/constants.h"
#include <stdexcept>
#include <boost/version.hpp>
#include <string>

#ifdef _WIN32
#define LIBLSL_CPP_API __declspec(dllexport)
#else
#define LIBLSL_CPP_API
#endif

#ifdef _MSC_VER
#pragma warning( disable : 4275 )
#if _MSC_VER <= 1600
#include <boost/cstdint.hpp>
using lslboost::int8_t;
using lslboost::int16_t;
using lslboost::int32_t;
using lslboost::int64_t;
using lslboost::uint8_t;
using lslboost::uint16_t;
using lslboost::uint32_t;
using lslboost::uint64_t;
#define __func__ "An unknown function"
#endif
#endif

#ifndef BOOST_CSTDINT_HPP
#include <cstdint>
#endif

#if BOOST_VERSION < 104500
	#error "Please do not compile this with a lslboost version older than 1.45 because the library would otherwise not be protocol-compatible with builds using other versions."
#endif

// the highest supported protocol version
// * 100 is the original version, supported by library versions 1.00+
// * 110 is an alternative protocol that improves throughput, supported by library versions 1.10+
const int LSL_PROTOCOL_VERSION = 110;

// the library version
const int LSL_LIBRARY_VERSION = 113;

namespace lsl {
	/// A very large time duration (> 1 year) for timeout values.
	const double FOREVER = 32000000.0;

	/// Constant to indicate that a sample has the next successive time stamp.
	const double DEDUCED_TIMESTAMP = -1.0; 

	/// Constant to indicate that a stream has variable sampling rate.
	const double IRREGULAR_RATE = 0.0; 

	/// Obtain a local system time stamp in seconds.
	double lsl_clock();

	/// Ensure that LSL is initialized.
	void ensure_lsl_initialized();

	/// Exception class that indicates that a stream inlet's source has been irrecoverably lost.
	class LIBLSL_CPP_API lost_error: public std::runtime_error {
	public:
		explicit lost_error(const std::string &msg): std::runtime_error(msg) {}
	};


	/// Exception class that indicates that an operation failed due to a timeout.
	class LIBLSL_CPP_API timeout_error: public std::runtime_error {
	public:
		explicit timeout_error(const std::string &msg): std::runtime_error(msg) {}
	};
}
#endif
