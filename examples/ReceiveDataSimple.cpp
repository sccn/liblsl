#include <iostream>
#include <lsl_cpp.h>
#include <vector>

/**
 * This is a minimal example that demonstrates how a multi-channel stream (here 128ch) of a
 * particular name (here: SimpleStream) can be resolved into an inlet, and how the raw sample data &
 * time stamps are pulled from the inlet. This example does not display the obtained data.
 */

int main(int argc, char **argv) {
	using namespace lsl;

	// resolve the stream of interest & make an inlet to get data from the first result
	std::vector<stream_info> results = resolve_stream("name", argc > 1 ? argv[1] : "SimpleStream");
	stream_inlet inlet(results.at(0));

	// receive data & time stamps forever (not displaying them here)
	std::vector<float> sample;
	while (true) {
		double timestamp = inlet.pull_sample(sample);
		std::cout << timestamp << "\t" << sample[0] << "\t" << sample[1] << "..." << std::endl;
	}

	return 0;
}
