#include <iostream>
#include <lsl_cpp.h>
#include <stdint.h>


int main(int argc, char **argv) {
	try {

		std::string name{argc > 1 ? argv[1] : "MyAudioStream"};
		// resolve the stream of interest & make an inlet
		lsl::stream_inlet inlet(lsl::resolve_stream("name", name).at(0));

		inlet.flush();

		double starttime = lsl::local_clock(), next_display = starttime + 1;

		// and retrieve the chunks
		uint64_t k = 0, num_samples = 0;
		while (true) {
			std::vector < std::vector<int16_t> > result;
			if (double timestamp = inlet.pull_chunk(result))
				num_samples += result.size();
			k++;

			// display code
			if (k % 50 == 0) {
				double now = lsl::local_clock();
				if (now > next_display) {
					std::cout << num_samples / (now - starttime) << " samples/sec" << std::endl;
					next_display = now + 1;
				}
			}
		}

	} catch (std::exception &e) { std::cerr << "Got an exception: " << e.what() << std::endl; }
	std::cout << "Press any key to exit. " << std::endl;
	std::cin.get();
	return 0;
}
