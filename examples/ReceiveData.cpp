#include <iostream>
#include <lsl_cpp.h>
#include <string>
#include <thread>


/**
 * Example program that demonstrates how to resolve a specific stream on the lab
 * network and how to connect to it in order to receive data.
 */

void printChunk(const std::vector<float> &chunk, std::size_t n_channels) {
	for (std::size_t i = 0; i < chunk.size(); ++i) {
		std::cout << chunk[i] << ' ';
		if (i % n_channels == n_channels - 1) std::cout << '\n';
	}
}

void printChunk(const std::vector<std::vector<float>> &chunk) {
	for (const auto &vec : chunk) printChunk(vec, vec.size());
}

int main(int argc, char *argv[]) {
	std::string field, value;
	const int max_samples = argc > 3 ? std::stoi(argv[3]) : 10;
	if (argc < 3) {
		std::cout << "This connects to a stream which has a particular value for a "
					 "given field and receives data.\nPlease enter a field name and the desired "
					 "value (e.g. \"type EEG\" (without the quotes)):"
				  << std::endl;
		std::cin >> field >> value;
	} else {
		field = argv[1];
		value = argv[2];
	}

	// resolve the stream of interet
	std::cout << "Now resolving streams..." << std::endl;
	std::vector<lsl::stream_info> results = lsl::resolve_stream(field, value);
	if (results.empty()) throw std::runtime_error("No stream found");

	std::cout << "Here is what was resolved: " << std::endl;
	std::cout << results[0].as_xml() << std::endl;

	// make an inlet to get data from it
	std::cout << "Now creating the inlet..." << std::endl;
	lsl::stream_inlet inlet(results[0]);

	// start receiving & displaying the data
	std::cout << "Now pulling samples..." << std::endl;

	std::vector<float> sample;
	std::vector<std::vector<float>> chunk_nested_vector;

	for (int i = 0; i < max_samples; ++i) {
		// pull a single sample
		inlet.pull_sample(sample);
		printChunk(sample, inlet.get_channel_count());

		// Sleep so the outlet will have time to push some samples
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		// pull a chunk into a nested vector - easier, but slower
		inlet.pull_chunk(chunk_nested_vector);
		printChunk(chunk_nested_vector);

		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		// pull a multiplexed chunk into a flat vector
		inlet.pull_chunk_multiplexed(sample);
		printChunk(sample, inlet.get_channel_count());
	}

	if (argc == 1) {
		std::cout << "Press any key to exit. " << std::endl;
		std::cin.get();
	}
	return 0;
}
