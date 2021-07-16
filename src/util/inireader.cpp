#include "inireader.hpp"
#include <istream>
#include <stdexcept>

void INI::load(std::istream &infile) {
	std::string line;
	std::string section;
	int linenr = 0;
	static const char ws[] = " \t\r\n";

	while (std::getline(infile, line)) {
		linenr++;
		// Comment / empty line
		if (line[0] == ';' || line.find_first_not_of(ws) == std::string::npos) continue;
		// Section
		if (line[0] == '[') {
			std::size_t closingbracket = line.find(']');
			if (closingbracket == std::string::npos)
				throw std::runtime_error(
					"No closing bracket ] found in line " + std::to_string(linenr));
			line[closingbracket] = '.';
			section = line.substr(1, closingbracket);
			continue;
		}
		// Key / Value - Pair
		std::size_t eqpos = line.find('=');
		if (eqpos == std::string::npos)
			throw std::runtime_error("No Key-Value pair in line " + std::to_string(linenr));
		auto keybegin = line.find_first_not_of(ws), keyend = line.find_last_not_of(ws, eqpos - 1),
			 valbegin = line.find_first_not_of(ws, eqpos + 1), valend = line.find_last_not_of(ws);
		if (keyend == std::string::npos)
			throw std::runtime_error("Empty key in line " + std::to_string(linenr));
		if (valbegin == std::string::npos || valend == eqpos)
			throw std::runtime_error("Empty value in line " + std::to_string(linenr));

		std::string key = section;
		key += line.substr(keybegin, keyend - keybegin + 1);
		if (values.find(key) != values.end()) throw std::runtime_error("Duplicate key " + key);
		values.insert(std::make_pair(key, line.substr(valbegin, valend - valbegin + 1)));
	}
}
