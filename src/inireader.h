#include <sstream>
#include <unordered_map>

class INI {
	std::unordered_map<std::string, std::string> values;

	template <typename T> inline T convert(const std::string &val) {
		std::istringstream is(val);
		T res;
		is >> res;
		return res;
	}

public:
	void load(std::istream &ini);

	template <typename T> inline T get(const std::string &key, T defaultval = T()) {
		auto it = values.find(key);
		if (it == values.end())
			return defaultval;
		else {
			return convert<T>(it->second);
		}
	}
};

template <> inline const char *INI::convert(const std::string &val) { return val.c_str(); }

template <> inline const std::string &INI::convert(const std::string &val) { return val; }
