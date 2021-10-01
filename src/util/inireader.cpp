#include "inireader.hpp"
#include <stdexcept>

void INI::load(std::istream &infile) {
	CallbackINIReader reader;
	reader.load(infile, [&values=this->values](std::string& section, std::string& key, const std::string &value) {
		if (values.find(key) != values.end()) throw std::runtime_error("Duplicate key " + key);
		if(!section.empty()) {
			section+='.';
			section+=key;
		}
		else section.swap(key);
		values.insert(std::make_pair(section, value));
	});
}
