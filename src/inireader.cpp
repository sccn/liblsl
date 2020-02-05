#include "inireader.h"
#include <algorithm>
#include <cctype>

void INI::load(std::istream & infile) {
	std::string line;
	std::string section;
	int linenr = 0;

	while (std::getline(infile, line)) {
		linenr++;
		// Comment / empty line
		if (line[0] == ';' || std::all_of(line.cbegin(), line.cend(), ::isspace)) continue;
		// Section
		if (line[0] == '[') {
			auto closingbracket = std::find(line.cbegin(), line.cend(), ']');
			if (closingbracket == line.cend())
				throw std::runtime_error(
						"No closing bracket ] found in line " + std::to_string(linenr));
			section = (std::string(line.cbegin() + 1, closingbracket) += '.');
			continue;
		}
		// Key / Value - Pair
		auto eqpos = line.find('=');
		if (eqpos == std::string::npos)
			throw std::runtime_error("No Key-Value pair in line " + std::to_string(linenr));
		const char *ws = " \t\r\n";
		auto keybegin = line.find_first_not_of(ws),
				keyend = line.find_last_not_of(ws, eqpos - 1),
				valbegin = line.find_first_not_of(ws, eqpos + 1),
				valend = line.find_last_not_of(ws);
		if (keyend == std::string::npos)
			throw std::runtime_error("Empty key in line " + std::to_string(linenr));
		if (valbegin == std::string::npos || valend == eqpos)
			throw std::runtime_error("Empty value in line " + std::to_string(linenr));

		auto key = section;
		key += line.substr(keybegin, keyend - keybegin + 1);
		if (values.find(key) != values.end()) throw std::runtime_error("Duplicate key " + key);
		values.insert(std::make_pair(
						  key, std::string(line.cbegin() + valbegin, line.cbegin() + valend + 1)));
	}
}
