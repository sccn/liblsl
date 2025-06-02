#include "../common/create_streampair.hpp"
#include "../common/lsltypes.hpp"
#include <atomic>
#include <catch2/catch_all.hpp>
#include <list>
#include <lsl_cpp.h>
#include <string>
#include <thread>

// clazy:excludeall=non-pod-global-static

template <typename T> struct sample_value { static const T val; };
template <> const char sample_value<char>::val = 122;
template <> const int64_t sample_value<int64_t>::val = 1LL << 62;
template <> const double sample_value<double>::val = 17324412.552;
template <> const std::string sample_value<std::string>::val(200, 'a');


TEMPLATE_TEST_CASE("pushpull", "[basic][throughput]", char, double, std::string) {
	const std::size_t max_nchan = 128, chunk_size = 128;
	const std::size_t param_nchan[] = {1, max_nchan};
	const std::size_t param_inlets[] = {0, 1, 10};

	const TestType data[max_nchan * chunk_size] = {sample_value<TestType>::val};

	const char *base_name = SampleType<TestType>::fmt_string();
	lsl::channel_format_t cf = (lsl::channel_format_t)SampleType<TestType>::chan_fmt;

	for (auto nchan : param_nchan) {
		for (auto n_inlets : param_inlets) {
			// Create a unique inlet name for each test combination
			auto now = std::chrono::high_resolution_clock::now();
			auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
			std::string unique_name = std::string(base_name) + "_" + 
									 std::to_string(nchan) + "_" + 
									 std::to_string(n_inlets) + "_" + 
									 std::to_string(timestamp);

			// Create outlet in its own scope
			std::unique_ptr<lsl::stream_outlet> out;
			std::vector<std::unique_ptr<lsl::stream_inlet>> inlets;

			try {
				out = std::make_unique<lsl::stream_outlet>(
					lsl::stream_info(unique_name, "PushPull", (int)nchan, chunk_size, cf, unique_name + "_src"));

				// Wait for outlet
				std::this_thread::sleep_for(std::chrono::milliseconds(200));

				// Resolve the stream
				auto found_stream_info = lsl::resolve_stream("name", unique_name, 1, 3.0);
				if (found_stream_info.empty()) {
					WARN("Could not resolve stream " << unique_name);
					continue;
				}

				// Create inlets
				for (std::size_t i = 0; i < n_inlets; ++i) {
					inlets.emplace_back(std::make_unique<lsl::stream_inlet>(found_stream_info[0], 300, 0, false));
					inlets.back()->open_stream(1.0);
				}

				// Wait for all consumers to connect
				if (n_inlets > 0) {
					auto start_wait = std::chrono::steady_clock::now();
					while (!out->have_consumers() && 
						   std::chrono::steady_clock::now() - start_wait < std::chrono::seconds(2)) {
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
					}
				}
				std::string suffix(std::to_string(nchan) + "_inlets_" + std::to_string(n_inlets));

				BENCHMARK("push_sample_nchan_" + suffix) {
					for (size_t s = 0; s < chunk_size; s++) {
						out->push_sample(data);
					}
					for (auto &inlet : inlets) {
						inlet->flush();
					}
				};
				BENCHMARK("push_chunk_nchan_" + suffix) {
					out->push_chunk_multiplexed(data, chunk_size);
					for (auto &inlet : inlets) {
						inlet->flush();
					}
				};
			} catch (const std::exception& e) {
				WARN("Exception in benchmark: " << e.what());
			}
			// Cleanup
			for (auto& inlet : inlets) {
				try {
					inlet->close_stream();
				} catch (...) {
					// Ignore cleanup errors
				}
			}
			inlets.clear();
			out.reset();
			// Give extra time for cleanup
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
}

TEMPLATE_TEST_CASE("stringconversion", "[basic][throughput]", int64_t, double) {
	const auto nchan = 16u, chunksize = 100u, nitems = chunksize * nchan;
	Streampair sp{create_streampair(lsl::stream_info("TypeConversionBench", "int2str2int",
		(int)nchan, chunksize, lsl::cf_string, "TypeConversion"))};
	std::vector<TestType> data(nitems, sample_value<TestType>::val);

	BENCHMARK("push") {
		sp.out_.push_chunk_multiplexed(data.data(), nitems);
		sp.in_.flush();
	};
	BENCHMARK("pushpull") {
		sp.out_.push_chunk_multiplexed(data.data(), nitems);
		sp.in_.pull_chunk_multiplexed(data.data(), nullptr, nitems, 0, 5.0);
	};
}
