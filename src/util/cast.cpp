#include "cast.hpp"
#include <iomanip>
#include <locale>
#include <sstream>

namespace lsl {

template <> std::string to_string(double val) {
	std::ostringstream os;
	os.imbue(std::locale::classic());
	os << std::setprecision(16) << std::showpoint << val;
	return os.str();
}

template <> std::string to_string(float val) {
	std::ostringstream os;
	os.imbue(std::locale::classic());
	os << std::setprecision(8) << std::showpoint << val;
	return os.str();
}

template <typename T> T from_string(const std::string &str) {
	std::istringstream is(str);
	is.imbue(std::locale::classic());
	T res;
	is >> res;
	return res;
}

// Explicit template instantiations so from_string doesn't have to be compiled
// in every compilation unit
template float from_string(const std::string &);
template double from_string(const std::string &);

template signed char from_string(const std::string &);
template char from_string(const std::string &);
template int16_t from_string(const std::string &);
template int32_t from_string(const std::string &);
template uint32_t from_string(const std::string &);
template int64_t from_string(const std::string &);
} // namespace lsl
