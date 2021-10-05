#include "cast.hpp"
#include <iosfwd>
#include <istream>
#include <string>
#include <unordered_map>

template <typename Fn> void load_ini_file(std::istream &infile, Fn &&callback) {
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
			section = line.substr(1, closingbracket - 1);
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

		std::string key = line.substr(keybegin, keyend - keybegin + 1);
		std::string value = line.substr(valbegin, valend - valbegin + 1);
		callback(section, key, value);
	}
}
