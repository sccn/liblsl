#include "cast.h"
#include <boost/lexical_cast.hpp>
#include <iomanip>
#include <locale>
#include <sstream>

namespace lsl {

template <> std::string to_string(double str) {
	std::ostringstream os;
	os.imbue(std::locale::classic());
	os << std::setprecision(16) << std::showpoint << str;
	return os.str();
}

template <> std::string to_string(float str) {
	std::ostringstream os;
	os.imbue(std::locale::classic());
	os << std::setprecision(8) << std::showpoint << str;
	return os.str();
}

template <typename T> T from_string(const std::string &str) {
	return lslboost::lexical_cast<T>(str);
}

template <> double from_string(const std::string &str) {
	std::istringstream is(str);
	is.imbue(std::locale::classic());
	double res;
	is >> res;
	return res;
}

template <> float from_string(const std::string &str) {
	return static_cast<float>(from_string<double>(str));
}

// Explicit template instantiations so lexical_cast doesn't have to be included
// in every compilation unit
template signed char from_string(const std::string&);
template char from_string(const std::string&);
template int16_t from_string(const std::string&);
template int32_t from_string(const std::string&);
template int64_t from_string(const std::string&);
}
