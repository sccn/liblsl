#include "lsl_cpp.h"
#include <chrono>
#include <iostream>
#include <random>
#include <cstdlib>
#include <thread>

using namespace std::chrono_literals;

const char *channels[] = {"C3", "C4", "Cz", "FPz", "POz", "CPz", "O1", "O2"};

int main(int argc, char *argv[]) {
	int n_channels = argc > 1 ? std::stoi(argv[1]) : 200;
	int samplingrate = argc > 2 ? std::stoi(argv[2]) : 50000;
	int max_buffered = argc > 3 ? std::stoi(argv[3]) : 360;
	bool sync = argc > 4 ? std::stoi(argv[4]) > 0 : true;

	try {
		lsl::stream_info info("Spike", "bench", n_channels, samplingrate, lsl::cf_int16, "Spike");
		lsl::stream_outlet outlet(
			info, 0, max_buffered, sync ? transp_sync_blocking : transp_default);

		std::vector<int16_t> chunk(n_channels * samplingrate / 1000, 5);

		const auto t_start = std::chrono::high_resolution_clock::now();
		auto next_sample_time = t_start;
		const auto time_per_chunk = std::chrono::microseconds(8 / samplingrate);

		// send data forever
		std::cout << "Now sending data... " << std::endl;
		for (unsigned t = 0;; t++) {
			if (t % 3 == 0) {
				for (int s = 0; s < 5; ++s)
					outlet.push_chunk_multiplexed(chunk);
				// Wait until the next expected sample time.
			} else {
				for (int s = 0; s < 10; ++s) {
					outlet.push_sample(chunk.data());
					std::this_thread::sleep_for(100us);
				}
			}
			next_sample_time += time_per_chunk;
			std::this_thread::sleep_until(next_sample_time);
		}

	} catch (std::exception &e) { std::cerr << "Got an exception: " << e.what() << std::endl; }
	std::cout << "Press any key to exit. " << std::endl;
	std::cin.get();
	return 0;
}
