#include "cast.h"
#include <locale>
#include <sstream>

template <> std::string lsl::to_string(float src) {
	std::ostringstream os;
	os.imbue(std::locale::classic());
	os.precision(12);
	os << src;
	return os.str();
}

template <> std::string lsl::to_string(double src) {
	std::ostringstream os;
	os.imbue(std::locale::classic());
	os.precision(12);
	os << src;
	return os.str();
}

template <> float lsl::from_string(const std::string &str) {
	std::istringstream is(str);
	is.imbue(std::locale::classic());
	float res;
	is >> res;
	return res;
}

template <> double lsl::from_string(const std::string &str) {
	std::istringstream is(str);
	is.imbue(std::locale::classic());
	double res;
	is >> res;
	return res;
}
