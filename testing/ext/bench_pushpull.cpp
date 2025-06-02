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

	const char *name = SampleType<TestType>::fmt_string();
	lsl::channel_format_t cf = (lsl::channel_format_t)SampleType<TestType>::chan_fmt;

	for (auto nchan : param_nchan) {
		// Create outlet with a unique name for each test iteration
		std::string unique_name = std::string(name) + "_" + std::to_string(nchan) + "_" + 
								 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
		lsl::stream_outlet out(
			lsl::stream_info(unique_name, "PushPull", (int)nchan, chunk_size, cf, "streamid"));
		
		// Wait for outlet to be discoverable
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		
		auto found_stream_info(lsl::resolve_stream("name", unique_name, 1, 2.0));
		REQUIRE(!found_stream_info.empty());

		for (auto n_inlets : param_inlets) {
			std::vector<std::unique_ptr<lsl::stream_inlet>> inlets;
			
			// Create inlets
			for (std::size_t i = 0; i < n_inlets; ++i) {
				inlets.emplace_back(std::make_unique<lsl::stream_inlet>(found_stream_info[0], 300, false));
				inlets.back()->open_stream(.5);
			}
			
			// Wait for consumers to connect
			if (n_inlets > 0) {
				out.wait_for_consumers(1.0);
			}

			std::string suffix(std::to_string(nchan) + "_inlets_" + std::to_string(n_inlets));

			BENCHMARK("push_sample_nchan_" + suffix) {
				for (size_t s = 0; s < chunk_size; s++) out.push_sample(data);
				for (auto &inlet : inlets) inlet->flush();
			};

			BENCHMARK("push_chunk_nchan_" + suffix) {
				out.push_chunk_multiplexed(data, chunk_size);
				for (auto &inlet : inlets) inlet->flush();
			};

			// Explicitly close inlets and wait for cleanup
			for (auto &inlet : inlets) {
				inlet->close_stream();
			}
			inlets.clear();
			
			// Give time for network cleanup between iterations
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
