#include "catch.hpp"
#include "helper_type.hpp"
#include <atomic>
#include <list>
#include <lsl_cpp.h>
#include <string>
#include <thread>

TEMPLATE_TEST_CASE("pushpull", "[basic][throughput]", char, float, std::string) {
	const std::size_t max_nchan = 128, chunk_size = 128;
	const std::size_t param_nchan[] = {1, max_nchan};
	const std::size_t param_inlets[] = {0, 1, 10};

	const TestType data[max_nchan * chunk_size] = {TestType()};
	// TestType data_out[max_n * chunk_size];

	const char *name = SampleType<TestType>::fmt_string();
	lsl::channel_format_t cf = (lsl::channel_format_t)SampleType<TestType>::chan_fmt;

	for (auto nchan : param_nchan) {
		lsl::stream_outlet out(
			lsl::stream_info(name, "PushPull", (int)nchan, lsl::IRREGULAR_RATE, cf, "streamid"));
		auto found_stream_info(lsl::resolve_stream("name", name, 1, 2.0));
		REQUIRE(!found_stream_info.empty());

		std::list<lsl::stream_inlet> inlet_list;
		for (auto n_inlets : param_inlets) {
			while (inlet_list.size() < n_inlets) {
				inlet_list.emplace_front(found_stream_info[0], 1, false);
				inlet_list.front().open_stream(.5);
			}

			BENCHMARK("push_sample_nchan_" + std::to_string(nchan) + "_inlets_" +
					  std::to_string(n_inlets)) {
				for (size_t s = 0; s < chunk_size; s++) out.push_sample(data);
			};

			BENCHMARK("push_chunk_nchan_" + std::to_string(nchan) + "_inlets_" +
					  std::to_string(n_inlets)) {
				out.push_chunk_multiplexed(data, chunk_size);
			};
			for (auto &inlet : inlet_list) inlet.flush();
		}
	}
}
