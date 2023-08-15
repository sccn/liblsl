#include "cast.hpp"
#include <iosfwd>
#include <string>
#include <unordered_map>

// Reads an INI file from a stream into a map
class INI {
	std::unordered_map<std::string, std::string> values;

public:
	void load(std::istream &infile);
	template <typename T> T get(const char *key, T defaultval = T());
};

// specialization for const char*, returns an empty string ("") by default instead of nullptr
template <> const char *INI::get<const char *>(const char *key, const char *defaultval);
