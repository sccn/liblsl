#include <cmath>
#include <iostream>
#include <lsl_cpp.h>
#include <thread>

using Clock = std::chrono::high_resolution_clock;

int main(int argc, char **argv) {
	try {
		const char *name = argc > 1 ? argv[1] : "MultiStream";
		const int rate = argc > 2 ? std::stoi(argv[2]) : 1000;
		std::vector<lsl::stream_outlet> outlets;
		for (auto fmt :
			{lsl::cf_int16, lsl::cf_int32, lsl::cf_int64, lsl::cf_double64, lsl::cf_string})
			outlets.emplace_back(
				lsl::stream_info(name + std::to_string(fmt), "Example", 1, rate, fmt), 0, 360);

		std::cout << "Now sending data..." << std::endl;
		std::vector<int32_t> mychunk(rate);

		auto nextsample = Clock::now();
		for (int c = 0; c < rate * 600;) {
			// increment the sample counter
			for (auto &sample : mychunk) sample = c++;

			nextsample += std::chrono::seconds(1);
			std::this_thread::sleep_until(nextsample);
			double ts = lsl::local_clock();

			for (auto &outlet : outlets) outlet.push_chunk_multiplexed(mychunk, ts);
		}

	} catch (std::exception &e) { std::cerr << "Got an exception: " << e.what() << std::endl; }
	std::cout << "Press any key to exit. " << std::endl;
	std::cin.get();
	return 0;
}
