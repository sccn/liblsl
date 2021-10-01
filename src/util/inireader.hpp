#include "cast.hpp"
#include <iosfwd>
#include <istream>
#include <string>
#include <unordered_map>

class CallbackINIReader {
private:

	public:
	template <typename Fn> void load(std::istream& infile, Fn&& callback) {
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
};

// Reads an INI file from a stream into a map
class INI {
	std::unordered_map<std::string, std::string> values;

	template <typename T> inline T convert(const std::string &val) {
		return lsl::from_string<T>(val);
	}

public:
	void load(std::istream &infile);

	template <typename T> inline T get(const char *key, T defaultval = T()) {
		auto it = values.find(key);
		if (it == values.end())
			return defaultval;
		else {
			return convert<T>(it->second);
		}
	}
};

// specialization for const char*, returns an empty string ("") by default instead of nullptr
template <> inline const char *INI::get<const char *>(const char *key, const char *defaultval) {
	static const char empty[] = "";
	auto it = values.find(key);
	return it == values.end() ? (defaultval ? defaultval : empty) : it->second.c_str();
}
