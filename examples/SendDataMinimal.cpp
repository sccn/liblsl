#include <chrono>
#include <thread>
#include <lsl_cpp.h>

/**
 * This is a minimal example of how a multi-channel data stream can be sent to LSL.
 * Here, the stream is named SimpleStream, has content-type EEG, 8 channels, and 200 Hz.
 * The transmitted samples contain random numbers.
 */

const int nchannels = 8;

int main(int argc, char *argv[]) {
	// make a new stream_info and open an outlet with it
	lsl::stream_info info("SimpleStream", "EEG", nchannels, 200.0);
	lsl::stream_outlet outlet(info);

	// send data forever
	float sample[nchannels];
	while (true) {
		// generate random data
		for (int c = 0; c < nchannels; c++) sample[c] = (float)((rand() % 1500) / 500.0 - 1.5);
		// send it
		outlet.push_sample(sample);
		// maintain our desired sampling rate (approximately; real software might do something else)
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	return 0;
}
