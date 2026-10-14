// Exercises the parts of liblsl that pull in its dependencies:
// pugixml (stream_info XML) and the networking libraries (outlet).
#include <lsl_cpp.h>
#include <iostream>

int main() {
	lsl::stream_info info("consumer_cpp", "Test", 1, 100, lsl::cf_float32, "consumer_cpp_uid");
	info.desc().append_child_value("manufacturer", "LSL");
	lsl::stream_outlet outlet(info);
	float sample = 1.f;
	outlet.push_sample(&sample);
	std::cout << "liblsl " << lsl::library_version() << " (" << lsl::library_info() << ")\n"
			  << "stream xml: " << outlet.info().as_xml().size() << " bytes" << std::endl;
	return 0;
}
