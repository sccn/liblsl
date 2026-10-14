#pragma once
extern "C" {
#include "../include/lsl/common.h"
}
#include <cstdint>
#include <string>
#include <type_traits>

// Bit-walking helpers for the data type tests. Integers are computed in uint64_t because shifting
// a negative value and negating the minimum are UB before C++20; the results are the same
// two's-complement values.
template <typename T> T shift_left_wrapping(T v) {
	return static_cast<T>(
		static_cast<int64_t>(static_cast<uint64_t>(static_cast<int64_t>(v)) << 1));
}
template <typename T> T one_minus_wrapping(T v) {
	return std::is_floating_point<T>::value
			   ? static_cast<T>(1 - v)
			   : static_cast<T>(
					 static_cast<int64_t>(1 - static_cast<uint64_t>(static_cast<int64_t>(v))));
}

template <typename T> struct SampleType {
	static const lsl_channel_format_t chan_fmt = cft_undefined;
	static const char *fmt_string();
};

template <> struct SampleType<int8_t> {
	static const lsl_channel_format_t chan_fmt = cft_int8;
	static const char *fmt_string() { return "cf_int8"; }
};

template <> struct SampleType<int16_t> {
	static const lsl_channel_format_t chan_fmt = cft_int16;
	static const char *fmt_string() { return "cf_int16"; }
};

template <> struct SampleType<int32_t> {
	static const lsl_channel_format_t chan_fmt = cft_int32;
	static const char *fmt_string() { return "cf_int32"; }
};

template <> struct SampleType<int64_t> {
	static const lsl_channel_format_t chan_fmt = cft_int64;
	static const char *fmt_string() { return "cf_int64"; }
};

template <> struct SampleType<float> {
	static const lsl_channel_format_t chan_fmt = cft_float32;
	static const char *fmt_string() { return "cf_float32"; }
};

template <> struct SampleType<double> {
	static const lsl_channel_format_t chan_fmt = cft_double64;
	static const char *fmt_string() { return "cf_double64"; }
};

template <> struct SampleType<std::string> {
	static const lsl_channel_format_t chan_fmt = cft_string;
	static const char *fmt_string() { return "cf_string"; }
};
