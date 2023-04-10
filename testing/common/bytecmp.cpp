#include "bytecmp.hpp"
#include <algorithm>
#include <catch2/catch_all.hpp>
#include <cctype>
#include <iomanip>
#include <map>
#include <sstream>

std::string bytes_to_hexstr(const std::string &str) {
	const std::map<char, char> shortcuts{{0x07, 'a'}, {0x08, 'b'}, {0x09, 't'}, {0x0a, 'n'},
		{0x0b, 'v'}, {0x0c, 'f'}, {0x0d, 'r'}, {0x5c, '\\'}, {0x22, '"'}, {0x27, '\''},
		{0x0a, 'n'}};
	std::ostringstream out;
	out << std::setfill('0');

	for (auto it = str.cbegin(); it != str.cend(); ++it) {
		char c = *it;
		char next = (it + 1) == str.cend() ? '\0' : *(it + 1);

		auto scit = shortcuts.find(c);
		if (scit != shortcuts.end())
			out << '\\' << scit->second;
		else if (std::isprint(c))
			out << c;
		else if (std::isxdigit(next))
			out << '\\' << std::oct << std::setw(3) << static_cast<int>(c);
		else if (c >= 0 && c < 8)
			out << '\\' << std::oct << std::setw(0) << static_cast<int>(c);
		else
			out << "\\x" << std::hex << std::setw(0) << (static_cast<int>(c) & 0xff);
	}
	return out.str();
}

void cmp_binstr(const std::string &a, const std::string b) {
	CHECK(a.size() == b.size());
	const int context_bytes = 8;
	auto diff = std::mismatch(a.begin(), a.end(), b.begin(), b.end());
	if (diff.first == a.end() && diff.second == b.end()) return;
	auto pos = diff.first - a.begin();
	if (pos < context_bytes)
		REQUIRE(bytes_to_hexstr(a.substr(0, context_bytes)) ==
				bytes_to_hexstr(b.substr(0, context_bytes)));
	else {
		INFO("First mismatch offset: " << pos);
		std::string cmp_a = bytes_to_hexstr(a.substr(0, context_bytes)) + " ... " +
							bytes_to_hexstr(a.substr(pos - context_bytes, 2 * context_bytes));
		std::string cmp_b = bytes_to_hexstr(b.substr(0, context_bytes)) + " ... " +
							bytes_to_hexstr(b.substr(pos - context_bytes, 2 * context_bytes));
		REQUIRE(cmp_a == cmp_b);
	}
}
