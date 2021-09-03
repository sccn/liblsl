#include <cmath>
#include <iostream>
#include <map>
#include <lsl_cpp.h>
#include <thread>
#include <algorithm>
#include <random>


// define a packed sample struct (here: a 16 bit stereo sample).
#pragma pack(1)
struct stereo_sample {
	int16_t l, r;
};

// fill buffer with data from device -- Normally your device SDK would provide such a function. Here we use a random number generator.
void get_data_from_device(std::vector<std::vector<int16_t>> buffer, uint64_t &sample_counter) {
	static std::uniform_int_distribution<int16_t> distribution(
		std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max());
	static std::default_random_engine generator;

	if (buffer[0].size() == 2) {
		// If there are only 2 channels then we'll do a sine wave, pretending this is an audio device.
		for (auto &frame : buffer) {
			frame[0] = static_cast<int16_t>(100 * sin(sample_counter / 200.));
			frame[1] = static_cast<int16_t>(120 * sin(sample_counter / 400.));
			sample_counter++;
		}
	}
	else {
		for (auto &frame : buffer) {
			for (std::size_t chan_idx = 0; chan_idx < frame.size(); ++chan_idx) {
				frame[chan_idx] = distribution(generator);
			}
			sample_counter++;
		}
	}
}

int main(int argc, char **argv) {
	std::cout << "SendDataInChunks" << std::endl;
	std::cout << "SendDataInChunks StreamName StreamType samplerate n_channels max_buffered chunk_rate" << std::endl;
	std::cout << "- max_buffered -- duration in sec (or x100 samples if samplerate is 0) to buffer for each outlet" << std::endl;
	std::cout << "- chunk_rate -- number of chunks pushed per second. For this example, make it a common factor of samplingrate and 1000." << std::endl;
	
	std::string name{argc > 1 ? argv[1] : "MyAudioStream"}, type{argc > 2 ? argv[2] : "Audio"};
	int samplingrate = argc > 3 ? std::stol(argv[3]) : 44100;
	int n_channels = argc > 4 ? std::stol(argv[4]) : 2;
	int32_t max_buffered = argc > 5 ? std::stol(argv[5]) : 360;
	int32_t chunk_rate = argc > 6 ? std::stol(argv[6]) : 10;  // Chunks per second.
	int32_t chunk_samples = samplingrate > 0 ? std::max((samplingrate / chunk_rate), 1) : 100;  // Samples per chunk.
	int32_t chunk_duration = 1000 / chunk_rate;  // Milliseconds per chunk

	try {
		lsl::stream_info info(name, type, n_channels, samplingrate, lsl::cf_int16);
		lsl::stream_outlet outlet(info, 0, max_buffered);
		lsl::xml_element desc = info.desc();
		desc.append_child_value("manufacturer", "LSL");
		lsl::xml_element chns = desc.append_child("channels");
		for (int c = 0; c < n_channels; c++) {
			lsl::xml_element chn = chns.append_child("channel");
			chn.append_child_value("label", "Chan-" + std::to_string(c));
			chn.append_child_value("unit", "microvolts");
			chn.append_child_value("type", "EEG");
		}

		// Prepare buffer to get data from 'device'
		std::vector<std::vector<int16_t>> chunk_buffer(
			chunk_samples,
			std::vector<int16_t>(n_channels));

		std::cout << "Now sending data..." << std::endl;

		// Your device might have its own timer. Or you can decide how often to poll
		//  your device, as we do here.
		auto nextsample = std::chrono::high_resolution_clock::now();
		uint64_t sample_counter = 0;
		for (unsigned c = 0;; c++) {

			// wait a bit
			nextsample += std::chrono::milliseconds(chunk_duration);
			std::this_thread::sleep_until(nextsample);

			// Get data from device
			get_data_from_device(chunk_buffer, sample_counter);

			// send it to the outlet
			outlet.push_chunk(chunk_buffer);
		}

	} catch (std::exception &e) { std::cerr << "Got an exception: " << e.what() << std::endl; }
	std::cout << "Press any key to exit. " << std::endl;
	std::cin.get();
	return 0;
}
