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
