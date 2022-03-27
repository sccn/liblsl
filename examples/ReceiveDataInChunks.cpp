#include <chrono>
#include <iostream>
#include <lsl_cpp.h>
#include <stdint.h>
#include <thread>


int main(int argc, char **argv) {
	std::cout << "ReceiveDataInChunks" << std::endl;
	std::cout << "ReceiveDataInChunks StreamName max_buflen flush" << std::endl;
	std::cout << "- max_buffered -- duration in sec (or x100 samples if samplerate is 0) to buffer "
				 "in the receiver"
			  << std::endl;
	std::cout
		<< "- flush -- set non-zero to flush data instead of pulling; useful for testing throughput"
		<< std::endl;

	try {

		std::string name{argc > 1 ? argv[1] : "MyAudioStream"};
		double max_buffered = argc > 2 ? std::stod(argv[2]) : 360.;
		bool flush = argc > 3;
		// resolve the stream of interest & make an inlet
		lsl::stream_info inlet_info = lsl::resolve_stream("name", name).at(0);
		lsl::stream_inlet inlet(inlet_info, (int32_t)max_buffered);

		// Use set_postprocessing to get the timestamps in a common base clock.
		// Do not use if this application will record timestamps to disk -- it is better to 
		//  do posthoc synchronization.
		inlet.set_postprocessing(lsl::post_ALL);

		// Inlet opening is implicit when doing pull_sample or pull_chunk.
		// Here we open the stream explicitly because we might be doing
		//  `flush` only.
		inlet.open_stream();

		double starttime = lsl::local_clock(), next_display = starttime + 1,
			   next_reset = starttime + 10;

		// and retrieve the chunks
		uint64_t k = 0, num_samples = 0;
		std::vector<std::vector<int16_t>> result;
		auto fetch_interval = std::chrono::milliseconds(20);
		auto next_fetch = std::chrono::steady_clock::now() + fetch_interval;


		while (true) {
			std::this_thread::sleep_until(next_fetch);
			if (flush) {
				// You almost certainly don't want to use flush. This is here so we
				//  can test maximum outlet throughput.
				num_samples += inlet.flush();
			} else {
				if (double timestamp = inlet.pull_chunk(result)) num_samples += result.size();
			}
			k++;
			next_fetch += fetch_interval;
			if (k % 50 == 0) {
				double now = lsl::local_clock();
				std::cout << num_samples / (now - starttime) << " samples/sec" << std::endl;
				if (now > next_reset) {
					std::cout << "Resetting counters..." << std::endl;
					starttime = now;
					next_reset = now + 10;
					num_samples = 0;
				}
			}
		}

	} catch (std::exception &e) { std::cerr << "Got an exception: " << e.what() << std::endl; }
	std::cout << "Press any key to exit. " << std::endl;
	std::cin.get();
	return 0;
}
