#include "lsl_cpp.h"
#include <array>
#include <chrono>
#include <iostream>
#include <stdlib.h>
#include <thread>

/**
 * This example program offers an 8-channel stream, float-formatted, that resembles EEG data.
 * The example demonstrates also how per-channel meta-data can be specified using the .desc() field
 * of the stream information object.
 *
 * Note that the timer used in the send loop of this program is not particularly accurate.
 */


const char *channels[] = {"C3", "C4", "Cz", "FPz", "POz", "CPz", "O1", "O2"};

int main(int argc, char *argv[]) {
	std::string name, type;
	if (argc < 3) {
		std::cout
			<< "This opens a stream under some user-defined name and with a user-defined content "
			   "type."
			<< std::endl;
		std::cout << "SendData Name Type [n_channels=8] [srate=100] [max_buffered=360]"
				  << std::endl;
		std::cout
			<< "Please enter the stream name and the stream type (e.g. \"BioSemi EEG\" (without "
			   "the quotes)):"
			<< std::endl;
		std::cin >> name >> type;
	} else {
		name = argv[1];
		type = argv[2];
	}
	int n_channels = argc > 3 ? std::stol(argv[3]) : 8;
	n_channels = n_channels < 8 ? 8 : n_channels;
	int samplingrate = argc > 4 ? std::stol(argv[4]) : 100;
	int max_buffered = argc > 5 ? std::stol(argv[5]) : 360;

	try {

		// make a new stream_info (100 Hz)
		lsl::stream_info info(
			name, type, n_channels, samplingrate, lsl::cf_float32, std::string(name) += type);

		// add some description fields
		info.desc().append_child_value("manufacturer", "LSL");
		lsl::xml_element chns = info.desc().append_child("channels");
		for (int k = 0; k < n_channels; k++)
			chns.append_child("channel")
				.append_child_value("label", k < 8 ? channels[k] : "Chan-" + std::to_string(k + 1))
				.append_child_value("unit", "microvolts")
				.append_child_value("type", type);

		// make a new outlet
		lsl::stream_outlet outlet(info, 0, max_buffered);
		std::vector<float> sample(n_channels, 0.0);

		// Your device might have its own timer. Or you can decide how often to poll
		//  your device, as we do here.
		int32_t sample_dur_us = 1000000 / (samplingrate > 0 ? samplingrate : 100);
		auto t_start = std::chrono::high_resolution_clock::now();
		auto next_sample_time = t_start;

		// send data forever
		std::cout << "Now sending data... " << std::endl;
		double starttime = ((double)clock()) / CLOCKS_PER_SEC;
		for (unsigned t = 0;; t++) {
			// Create random data for the first 8 channels.
			for (int c = 0; c < 8; c++) sample[c] = (float)((rand() % 1500) / 500.0 - 1.5);
			// For the remaining channels, fill them with a sample counter (wraps at 1M).
			std::fill(sample.begin() + 8, sample.end(), (float)(t % 1000000));

			// Wait until the next expected sample time.
			next_sample_time += std::chrono::microseconds(sample_dur_us);
			std::this_thread::sleep_until(next_sample_time);

			// send the sample
			std::cout << sample[0] << "\t" << sample[n_channels-1] << std::endl;
			outlet.push_sample(sample);
		}

	} catch (std::exception &e) { std::cerr << "Got an exception: " << e.what() << std::endl; }
	std::cout << "Press any key to exit. " << std::endl;
	std::cin.get();
	return 0;
}
