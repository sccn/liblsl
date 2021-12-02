#include "cast.hpp"
#include <iosfwd>
#include <string>
#include <unordered_map>

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
		return convert<T>(it->second);
	}
};

// specialization for const char*, returns an empty string ("") by default instead of nullptr
template <> inline const char *INI::get<const char *>(const char *key, const char *defaultval) {
	static const char empty[] = "";
	auto it = values.find(key);
	return it == values.end() ? (defaultval ? defaultval : empty) : it->second.c_str();
}
