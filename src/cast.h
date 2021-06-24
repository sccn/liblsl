#pragma once
#include <string>

namespace lsl {
template <typename T> std::string to_string(T val) { return std::to_string(val); }
template <> std::string to_string(double val);
template <> std::string to_string(float val);

template <typename T> T from_string(const std::string &str);
template <> inline bool from_string(const std::string &str) { return str == "1"; }

} // namespace lsl
