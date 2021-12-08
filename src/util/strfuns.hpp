#pragma once

#include <string>
#include <vector>

namespace lsl {

inline bool isspace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

/// trim whitespace at the beginning of a sequence, return position of first non-whitespace character
template <typename IteratorType> IteratorType trim_begin(IteratorType begin, IteratorType end) {
	while (begin != end && lsl::isspace(*begin)) ++begin;
	return begin;
}

/// trim whitespace at the end of a sequence, return position one past the last non-whitespace character
template <typename IteratorType> IteratorType trim_end(IteratorType begin, IteratorType end) {
	while (end > begin && lsl::isspace(*(end - 1))) --end;
	return end;
}

/// remove whitespace from the beginning and end of a sequence, modifying both start and end iterators
template <typename IteratorType> void trim(IteratorType &begin, IteratorType &end) {
	end = trim_end(begin, end);
	begin = trim_begin(begin, end);
}

/// split a header line ("Foo-Bar: 512 ; some comment") into type ("foo-bar") and value ("512")
bool split_headerline(char *buf, std::size_t bufsize, std::string &type, std::string &value);

/// return a string with whitespace at the beginning and end trimmed
inline std::string trim(const std::string &input) {
	std::string::const_iterator begin = input.begin(), end = input.end();
	lsl::trim(begin, end);
	return {begin, end};
}

/// split a separated string like "this,is a,list" into its parts
std::vector<std::string> splitandtrim(
	const std::string &input, char separator = ',', bool keepempty = false);

} // namespace lsl
