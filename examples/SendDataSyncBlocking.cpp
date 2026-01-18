#include <chrono>
#include <cstdlib>
#include <iostream>
#include <lsl_cpp.h>
#include <thread>
#include <vector>

/**
 * This example demonstrates the synchronous (zero-copy) outlet mode.
 *
 * When using transp_sync_blocking, push_sample() blocks until data is written
 * to all connected consumers. This eliminates data copies (user buffer is sent
 * directly to the network socket) which reduces CPU usage for high-bandwidth streams.
 *
 * Trade-off: Call latency depends on network speed and number of consumers.
 *
 * Usage: SendDataSyncBlocking [stream_name] [num_channels] [sample_rate]
 *        Default: SyncStream, 64 channels, 1000 Hz
 */

int main(int argc, char *argv[]) {
	std::string name = argc > 1 ? argv[1] : "SyncStream";
	int nchannels = argc > 2 ? std::atoi(argv[2]) : 64;
	double srate = argc > 3 ? std::atof(argv[3]) : 1000.0;

	std::cout << "Creating sync outlet: " << name << " with " << nchannels << " channels @ "
			  << srate << " Hz\n";

	// Create stream info
	lsl::stream_info info(name, "EEG", nchannels, srate, lsl::cf_float32);

	// Create outlet with transp_sync_blocking flag for zero-copy transfer
	// Note: The third parameter (max_buffered) is less important in sync mode
	// since data goes directly to the socket without intermediate buffering.
	lsl::stream_outlet outlet(info, 0, 360, lsl::transp_sync_blocking);

	std::cout << "Waiting for consumers...\n";
	while (!outlet.wait_for_consumers(5)) {
		std::cout << "  (still waiting)\n";
	}
	std::cout << "Consumer connected! Starting data transmission.\n";

	// Allocate sample buffer
	std::vector<float> sample(nchannels);

	// Calculate sleep duration between samples
	auto sample_interval = std::chrono::duration<double>(1.0 / srate);
	auto next_sample_time = std::chrono::steady_clock::now();

	// Statistics
	uint64_t samples_sent = 0;
	auto start_time = std::chrono::steady_clock::now();

	// Send data while consumers are connected
	while (outlet.have_consumers()) {
		// Generate sample data (in real applications, this would be acquired from hardware)
		for (int c = 0; c < nchannels; c++) {
			sample[c] = static_cast<float>((std::rand() % 1000) / 500.0 - 1.0);
		}

		// Push sample - this BLOCKS until data is written to all consumers
		// The sample buffer is used directly (zero-copy) so it must remain valid
		// until push_sample returns.
		outlet.push_sample(sample);
		samples_sent++;

		// Print statistics every second
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration<double>(now - start_time).count();
		if (samples_sent % static_cast<uint64_t>(srate) == 0) {
			std::cout << "Sent " << samples_sent << " samples, effective rate: "
					  << (samples_sent / elapsed) << " Hz\n";
		}

		// Pace the data transmission
		next_sample_time += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
			sample_interval);
		std::this_thread::sleep_until(next_sample_time);
	}

	std::cout << "Consumer disconnected. Total samples sent: " << samples_sent << "\n";

	return 0;
}
