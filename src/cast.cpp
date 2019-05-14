#include "cast.h"
#include <iomanip>
#include <locale>
#include <sstream>

template <> std::string lsl::to_string(double str) {
	std::ostringstream os;
	os.imbue(std::locale::classic());
	os << std::setprecision(16) << std::showpoint << str;
	return os.str();
}

template <> std::string lsl::to_string(float str) {
	std::ostringstream os;
	os.imbue(std::locale::classic());
	os << std::setprecision(8) << std::showpoint << str;
	return os.str();
}

template <> double lsl::from_string(const std::string &str) {
	std::istringstream is(str);
	is.imbue(std::locale::classic());
	double res;
	is >> res;
	return res;
}

template <> float lsl::from_string(const std::string &str) {
	return static_cast<float>(lsl::from_string<double>(str));
}
