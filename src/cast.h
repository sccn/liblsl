#pragma once

#include <boost/lexical_cast.hpp>
#include <string>

namespace lsl {
template <typename T> inline std::string to_string(T str) {
	return lslboost::lexical_cast<std::string>(str);
}

template <> std::string to_string(double str);
template <> std::string to_string(float str);

template <typename T> inline T from_string(const std::string &str) {
	return lslboost::lexical_cast<T>(str);
}

template <> double from_string(const std::string &str);
template <> float from_string(const std::string &str);
} // namespace lsl
