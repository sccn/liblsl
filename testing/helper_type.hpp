#pragma once
extern "C" {
#include "../include/lsl/common.h"
}
#include <string>

template <typename T> struct SampleType {
	static const lsl_channel_format_t chan_fmt = cft_undefined;
	static const char *fmt_string();
};

template <> struct SampleType<char> {
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
