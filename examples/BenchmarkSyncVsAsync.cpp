#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <lsl_cpp.h>
#include <numeric>
#include <sys/resource.h>
#include <thread>
#include <vector>

/**
 * Benchmark comparing synchronous (zero-copy) vs asynchronous outlet performance.
 *
 * Measures:
 * - Push latency: Time for push_sample() to return
 * - CPU time: User and system CPU time consumed
 * - Throughput: Samples pushed per second
 *
 * Usage: BenchmarkSyncVsAsync [num_channels] [num_samples] [num_consumers] [sample_rate] [chunk_size]
 *        Default: 64 channels, 10000 samples, 1 consumer, 0 (unlimited), 1 (push_sample)
 *        Use sample_rate > 0 to pace the benchmark (e.g., 1000 for 1 kHz)
 *        Use chunk_size > 1 to test push_chunk instead of push_sample
 */

struct Stats {
	double min_us, max_us, mean_us, median_us, stddev_us;
	double total_ms;
	double throughput; // samples/sec
	double cpu_user_ms;   // user CPU time in ms
	double cpu_system_ms; // system CPU time in ms
};

// Get current CPU time (user, system) for this process in ms
std::pair<double, double> get_cpu_time_ms() {
	struct rusage usage;
	getrusage(RUSAGE_SELF, &usage);
	double user_ms = usage.ru_utime.tv_sec * 1000.0 + usage.ru_utime.tv_usec / 1000.0;
	double sys_ms = usage.ru_stime.tv_sec * 1000.0 + usage.ru_stime.tv_usec / 1000.0;
	return {user_ms, sys_ms};
}

Stats compute_stats(std::vector<double> &latencies_us, double total_time_ms, int num_samples,
	double cpu_user_ms, double cpu_system_ms) {
	Stats s{};
	if (latencies_us.empty()) return s;

	std::sort(latencies_us.begin(), latencies_us.end());
	s.min_us = latencies_us.front();
	s.max_us = latencies_us.back();
	s.median_us = latencies_us[latencies_us.size() / 2];

	double sum = std::accumulate(latencies_us.begin(), latencies_us.end(), 0.0);
	s.mean_us = sum / latencies_us.size();

	double sq_sum = 0;
	for (double v : latencies_us) { sq_sum += (v - s.mean_us) * (v - s.mean_us); }
	s.stddev_us = std::sqrt(sq_sum / latencies_us.size());

	s.total_ms = total_time_ms;
	s.throughput = num_samples / (total_time_ms / 1000.0);
	s.cpu_user_ms = cpu_user_ms;
	s.cpu_system_ms = cpu_system_ms;

	return s;
}

void print_stats(const char *label, const Stats &s, int nsamples) {
	std::cout << std::fixed << std::setprecision(2);
	std::cout << label << ":\n";
	std::cout << "  Latency (us): min=" << s.min_us << ", max=" << s.max_us << ", mean=" << s.mean_us
			  << ", median=" << s.median_us << ", stddev=" << s.stddev_us << "\n";
	std::cout << "  Wall time: " << s.total_ms << " ms, Throughput: " << std::setprecision(0)
			  << s.throughput << " samples/sec\n";
	double total_cpu = s.cpu_user_ms + s.cpu_system_ms;
	double cpu_per_sample_us = (total_cpu * 1000.0) / nsamples;
	std::cout << std::setprecision(2);
	std::cout << "  CPU time: " << total_cpu << " ms (user: " << s.cpu_user_ms
			  << ", sys: " << s.cpu_system_ms << "), " << cpu_per_sample_us << " us/sample\n";
}

// Consumer thread: pulls samples until signaled to stop
void consumer_thread(const std::string &stream_name, std::atomic<bool> &running,
	std::atomic<int> &samples_received) {
	try {
		auto found = lsl::resolve_stream("name", stream_name, 1, 10.0);
		if (found.empty()) {
			std::cout << "    [Consumer] ERROR: Could not find stream " << stream_name << "\n" << std::flush;
			return;
		}
		std::cout << "    [Consumer] Found stream, opening..." << std::flush;
		lsl::stream_inlet inlet(found[0]);
		inlet.open_stream(5.0);
		std::cout << " opened.\n" << std::flush;

		int nchannels = inlet.info().channel_count();
		std::vector<float> sample(nchannels);

		while (running) {
			double ts = inlet.pull_sample(sample, 0.1);
			if (ts != 0.0) { samples_received++; }
		}

		// Drain remaining samples
		while (inlet.pull_sample(sample, 0.01) != 0.0) { samples_received++; }
	} catch (std::exception &e) { std::cerr << "Consumer error: " << e.what() << "\n"; }
}

Stats run_benchmark(const std::string &name, int nchannels, int nsamples, int nconsumers,
	lsl_transport_options_t flags, double sample_rate = 0, int chunk_size = 1) {
	// Create outlet
	double nominal_rate = sample_rate > 0 ? sample_rate : lsl::IRREGULAR_RATE;
	lsl::stream_info info(name, "Benchmark", nchannels, nominal_rate, lsl::cf_float32, name);
	lsl::stream_outlet outlet(info, 0, 360, flags);

	// Start consumer threads
	std::atomic<bool> running{true};
	std::vector<std::atomic<int>> samples_received(nconsumers);
	std::vector<std::thread> consumers;

	for (int i = 0; i < nconsumers; i++) {
		samples_received[i] = 0;
		consumers.emplace_back(consumer_thread, name, std::ref(running), std::ref(samples_received[i]));
	}

	// Wait for consumers to connect
	std::cout << "  Waiting for " << nconsumers << " consumer(s)..." << std::flush;
	while (!outlet.wait_for_consumers(1.0)) { std::cout << "." << std::flush; }
	std::cout << " connected!\n" << std::flush;
	// Give sockets time to be handed off (for sync mode)
	std::this_thread::sleep_for(std::chrono::milliseconds(200));

	// Prepare sample/chunk buffer
	std::vector<float> chunk_buf(nchannels * chunk_size);
	for (int c = 0; c < nchannels * chunk_size; c++) { chunk_buf[c] = static_cast<float>(c % nchannels); }

	// Run benchmark
	std::vector<double> latencies_us;
	int num_pushes = (nsamples + chunk_size - 1) / chunk_size;  // ceiling division
	latencies_us.reserve(num_pushes);

	// Calculate pacing interval if sample_rate is specified
	std::chrono::nanoseconds chunk_interval_ns{0};
	if (sample_rate > 0) {
		chunk_interval_ns = std::chrono::nanoseconds(static_cast<int64_t>(1e9 * chunk_size / sample_rate));
		std::cout << "  Pushing " << nsamples << " samples";
		if (chunk_size > 1) std::cout << " (chunks of " << chunk_size << ")";
		std::cout << " @ " << sample_rate << " Hz..." << std::flush;
	} else {
		std::cout << "  Pushing " << nsamples << " samples";
		if (chunk_size > 1) std::cout << " (chunks of " << chunk_size << ")";
		std::cout << " (max speed)..." << std::flush;
	}

	// Measure CPU time before and after
	auto [cpu_user_start, cpu_sys_start] = get_cpu_time_ms();
	auto start = std::chrono::high_resolution_clock::now();
	auto next_chunk_time = start;

	int samples_pushed = 0;
	while (samples_pushed < nsamples) {
		// Pace if sample_rate is set
		if (sample_rate > 0) {
			std::this_thread::sleep_until(next_chunk_time);
			next_chunk_time += chunk_interval_ns;
		}

		// Determine actual chunk size for this push (may be smaller for last chunk)
		int this_chunk = std::min(chunk_size, nsamples - samples_pushed);

		auto t0 = std::chrono::high_resolution_clock::now();
		if (this_chunk == 1) {
			outlet.push_sample(chunk_buf.data());
		} else {
			outlet.push_chunk_multiplexed(chunk_buf.data(), this_chunk * nchannels);
		}
		auto t1 = std::chrono::high_resolution_clock::now();

		double latency_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
		latencies_us.push_back(latency_us);
		samples_pushed += this_chunk;
	}

	auto end = std::chrono::high_resolution_clock::now();
	auto [cpu_user_end, cpu_sys_end] = get_cpu_time_ms();

	double total_ms = std::chrono::duration<double, std::milli>(end - start).count();
	double cpu_user_ms = cpu_user_end - cpu_user_start;
	double cpu_sys_ms = cpu_sys_end - cpu_sys_start;
	std::cout << " done.\n" << std::flush;

	// Stop consumers
	running = false;
	for (auto &t : consumers) { t.join(); }

	// Report received samples
	int total_received = 0;
	for (int i = 0; i < nconsumers; i++) { total_received += samples_received[i].load(); }
	std::cout << "  Consumers received: " << total_received << "/" << (nsamples * nconsumers)
			  << " samples\n" << std::flush;

	return compute_stats(latencies_us, total_ms, nsamples, cpu_user_ms, cpu_sys_ms);
}

int main(int argc, char *argv[]) {
	int nchannels = argc > 1 ? std::atoi(argv[1]) : 64;
	int nsamples = argc > 2 ? std::atoi(argv[2]) : 10000;
	int nconsumers = argc > 3 ? std::atoi(argv[3]) : 1;
	double sample_rate = argc > 4 ? std::atof(argv[4]) : 0;  // 0 = unlimited
	int chunk_size = argc > 5 ? std::atoi(argv[5]) : 1;      // 1 = push_sample

	std::cout << "=== LSL Sync vs Async Outlet Benchmark ===\n";
	std::cout << "Channels: " << nchannels << ", Samples: " << nsamples
			  << ", Consumers: " << nconsumers;
	if (sample_rate > 0) {
		std::cout << ", Rate: " << sample_rate << " Hz";
	}
	if (chunk_size > 1) {
		std::cout << ", Chunk: " << chunk_size;
	}
	std::cout << "\n";
	std::cout << "Sample size: " << (nchannels * sizeof(float)) << " bytes\n\n" << std::flush;

	// Run async benchmark
	std::cout << "Running ASYNC benchmark...\n";
	Stats async_stats = run_benchmark("BenchAsync", nchannels, nsamples, nconsumers, transp_default, sample_rate, chunk_size);
	print_stats("ASYNC", async_stats, nsamples);
	std::cout << "\n";

	// Delay between tests for cleanup (outlets need time to fully shut down)
	std::cout << "Waiting for cleanup..." << std::flush;
	std::this_thread::sleep_for(std::chrono::seconds(2));
	std::cout << " done.\n" << std::flush;

	// Run sync benchmark
	std::cout << "Running SYNC benchmark...\n";
	Stats sync_stats =
		run_benchmark("BenchSync", nchannels, nsamples, nconsumers, transp_sync_blocking, sample_rate, chunk_size);
	print_stats("SYNC", sync_stats, nsamples);
	std::cout << "\n";

	// Summary comparison
	std::cout << "=== Summary ===\n";
	std::cout << std::fixed << std::setprecision(2);

	double async_cpu_total = async_stats.cpu_user_ms + async_stats.cpu_system_ms;
	double sync_cpu_total = sync_stats.cpu_user_ms + sync_stats.cpu_system_ms;
	double async_cpu_per_sample = (async_cpu_total * 1000.0) / nsamples;
	double sync_cpu_per_sample = (sync_cpu_total * 1000.0) / nsamples;

	std::cout << "CPU per sample:  ASYNC=" << async_cpu_per_sample << " us, SYNC=" << sync_cpu_per_sample
			  << " us (ratio: " << (sync_cpu_per_sample / async_cpu_per_sample) << "x)\n";
	std::cout << "Latency:         ASYNC=" << async_stats.mean_us << " us, SYNC=" << sync_stats.mean_us
			  << " us (ratio: " << (sync_stats.mean_us / async_stats.mean_us) << "x)\n";
	std::cout << "Throughput:      ASYNC=" << std::setprecision(0) << async_stats.throughput
			  << ", SYNC=" << sync_stats.throughput
			  << " samples/sec (ratio: " << std::setprecision(2)
			  << (sync_stats.throughput / async_stats.throughput) << "x)\n";

	return 0;
}
