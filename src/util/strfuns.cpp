#include "strfuns.hpp"

std::vector<std::string> lsl::splitandtrim(
	const std::string &input, char separator, bool keepempty) {
	std::vector<std::string> parts;
	auto it = input.cbegin();
	while (true) {
		// Skip whitespace in the beginning
		it = lsl::trim_begin(it, input.cend());

		// find the next separator or end of string
		auto endit = it;
		while (endit != input.end() && *endit != separator) ++endit;
		// mark beginning of next part if not at the end
		auto next = endit;

		// shrink the range so it doesn't include whitespace at the end
		endit = trim_end(it, endit);
		if (endit != it || keepempty) parts.emplace_back(it, endit);

		if (next != input.cend())
			it = next + 1;
		else
			break;
	}
	return parts;
}

bool lsl::split_headerline(char *buf, std::size_t bufsize, std::string &type, std::string &value) {
	char *end = buf + bufsize, *middle_it = nullptr;

	buf = trim_begin(buf, end);
	// find the end of the header line, i.e. the end of the buffer or the start of a comment
	for (auto it = buf; it != end; ++it) {
		if(!*it || *it == ';') {
			end = it;
			break;
		}
		else if(*it == ':') middle_it = it;
	}

	// no separator found?
	if (middle_it == nullptr) return false;

	auto *value_begin = middle_it + 1;
	trim(value_begin, end);

	// convert to lowercase
	for (auto it = buf; it != end; ++it)
		if (*it >= 'A' && *it <= 'Z') *it += 'a' - 'A';

	type.assign(buf, trim_end(buf, middle_it));
	value.assign(value_begin, end);

	return true;
}
