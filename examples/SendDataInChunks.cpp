#include <cmath>
#include <iostream>
#include <map>
#include <cstring>
#include <lsl_cpp.h>
#include <thread>
#include <algorithm>
#include <random>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


// define a packed sample struct (here: a 16 bit stereo sample).
#pragma pack(1)
struct stereo_sample {
	int16_t l, r;
};

struct fake_device {
	/*
	We create a fake device that will generate data. The inner details are not
	so important because typically it will be up to the real data source + SDK
	to provide a way to get data.
	*/
	std::size_t n_channels;
	double srate;
	std::size_t pattern_samples;
	std::size_t head;
	std::vector<int16_t> pattern;
	std::chrono::steady_clock::time_point last_time;

	fake_device(const int16_t n_channels, const float srate)
		: n_channels(n_channels), srate(srate), head(0) {
		pattern_samples = (std::size_t)(srate - 0.5) + 1;  // truncate OK.

		// Pre-allocate entire test pattern. The data _could_ be generated on the fly
		//  for a much smaller memory hit, but we also use this example application
		//  to test LSL Outlet performance so we want to reduce out-of-LSL CPU
		//  utilization.
		int64_t magnitude = std::numeric_limits<int16_t>::max();
		int64_t offset_0 = magnitude / 2;
		int64_t offset_step = magnitude / n_channels;
		pattern.reserve(pattern_samples * n_channels);
		for (auto sample_ix = 0; sample_ix < pattern_samples; ++sample_ix) {
			for (auto chan_ix = 0; chan_ix < n_channels; ++chan_ix) {
				// sin(2*pi*f*t), where f cycles from 1 Hz to Nyquist: srate / 2
				double f = (chan_ix + 1) % (int)(srate / 2);
				pattern.emplace_back(
					static_cast<int16_t>(
						offset_0 + chan_ix * offset_step +
						magnitude * sin(2 * M_PI * f * sample_ix / srate)));
			}
		}
		last_time = std::chrono::steady_clock::now();
	}

	std::vector<int16_t> get_data() {
		auto now = std::chrono::steady_clock::now();
		auto elapsed_nano =
			std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_time).count();
		std::size_t elapsed_samples = std::size_t(elapsed_nano * srate * 1e-9); // truncate OK.
		std::vector<int16_t> result;
		result.resize(elapsed_samples * n_channels);
		int64_t ret_samples = get_data(result);
		std::vector<int16_t> output(result.begin(), result.begin() + ret_samples);
		return output;
	}

	std::size_t get_data(std::vector<int16_t> &buffer, bool nodata = false) {
		auto now = std::chrono::steady_clock::now();
		auto elapsed_nano =
			std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_time).count();
		std::size_t elapsed_samples = std::size_t(elapsed_nano * srate * 1e-9); // truncate OK.
		elapsed_samples = std::min(elapsed_samples, (std::size_t)(buffer.size() / n_channels));
		if (nodata) {
			// The fastest but no patterns.
			// memset(&buffer[0], 23, buffer.size() * sizeof buffer[0]);
		} else {
			std::size_t end_sample = head + elapsed_samples;
			std::size_t nowrap_samples = std::min(pattern_samples - head, elapsed_samples);
			memcpy(&buffer[0], &(pattern[head]), nowrap_samples);
			if (end_sample > pattern_samples) {
				memcpy(&buffer[nowrap_samples], &(pattern[0]), elapsed_samples - nowrap_samples);
			}
		}
		head = (head + elapsed_samples) % pattern_samples;
		last_time += std::chrono::nanoseconds(int64_t(1e9 * elapsed_samples / srate));
		return elapsed_samples;
	}
};

int main(int argc, char **argv) {
	std::cout << "SendDataInChunks" << std::endl;
	std::cout << "SendDataInChunks StreamName StreamType samplerate n_channels max_buffered chunk_rate" << std::endl;
	std::cout << "- max_buffered -- duration in sec (or x100 samples if samplerate is 0) to buffer for each outlet" << std::endl;
	std::cout << "- chunk_rate -- number of chunks pushed per second. For this example, make it a common factor of samplingrate and 1000." << std::endl;
	
	std::string name{argc > 1 ? argv[1] : "MyAudioStream"}, type{argc > 2 ? argv[2] : "Audio"};
	int samplingrate = argc > 3 ? std::stol(argv[3]) : 44100;  // Here we specify srate, but typically this would come from the device.
	int n_channels = argc > 4 ? std::stol(argv[4]) : 2;        // Here we specify n_chans, but typically this would come from theh device.
	double max_buffered = argc > 5 ? std::stod(argv[5]) : 360.;
	int32_t chunk_rate = argc > 6 ? std::stol(argv[6]) : 10;  // Chunks per second.
	int32_t chunk_samples = samplingrate > 0 ? std::max((samplingrate / chunk_rate), 1) : 100;  // Samples per chunk.
	int32_t chunk_duration = 1000 / chunk_rate;  // Milliseconds per chunk

	try {
		// Prepare the LSL stream.
		lsl::stream_info info(
			name, type, n_channels, samplingrate, lsl::cf_int16, "example-SendDataInChunks");
		lsl::xml_element desc = info.desc();
		desc.append_child_value("manufacturer", "LSL");
		lsl::xml_element chns = desc.append_child("channels");
		for (int c = 0; c < n_channels; c++) {
			lsl::xml_element chn = chns.append_child("channel");
			chn.append_child_value("label", "Chan-" + std::to_string(c));
			chn.append_child_value("unit", "microvolts");
			chn.append_child_value("type", type);
		}
		int32_t buf_samples = (int32_t)(max_buffered * samplingrate);
		lsl::stream_outlet outlet(info, chunk_samples, buf_samples);
		info = outlet.info(); // Refresh info with whatever the outlet captured.
		std::cout << "Stream UID: " << info.uid() << std::endl;

		// Create a connection to our device.
		fake_device my_device(n_channels, (float)samplingrate);

		// Prepare buffer to get data from 'device'.
		//  The buffer should be larger than you think you need. Here we make it 4x as large.
		std::vector<int16_t> chunk_buffer(4 * chunk_samples * n_channels);

		std::cout << "Now sending data..." << std::endl;

		// Your device might have its own timer. Or you can decide how often to poll
		//  your device, as we do here.
		auto t_start = std::chrono::high_resolution_clock::now();
		auto next_chunk_time = t_start;
		for (unsigned c = 0;; c++) {
			// wait a bit
			next_chunk_time += std::chrono::milliseconds(chunk_duration);
			std::this_thread::sleep_until(next_chunk_time);

			// Get data from device
			std::size_t returned_samples = my_device.get_data(chunk_buffer);

			// send it to the outlet. push_chunk_multiplexed is one of the more complicated approaches.
			//  other push_chunk methods are easier but slightly slower.
			double ts = lsl::local_clock();
			outlet.push_chunk_multiplexed(
				chunk_buffer.data(), returned_samples * n_channels, ts, true);
		}
	} catch (std::exception &e) { std::cerr << "Got an exception: " << e.what() << std::endl; }
	std::cout << "Press any key to exit. " << std::endl;
	std::cin.get();
	return 0;
}
