#pragma once

#include <boost/lexical_cast.hpp>
#include <string>

namespace lsl {
template <typename T> inline std::string to_string(T src) {
	return lslboost::lexical_cast<std::string>(src);
}

template <> std::string to_string(float src);
template <> std::string to_string(double src);

template <typename T> inline T from_string(const std::string &str) {
	return lslboost::lexical_cast<T>(str);
}

template <> float from_string(const std::string &str);
template <> double from_string(const std::string &str);
} // namespace lsl
