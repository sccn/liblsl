#include "catch.hpp"
#include "helper_type.hpp"
#include "helpers.h"
#include <atomic>
#include <list>
#include <lsl_cpp.h>
#include <string>
#include <thread>

template<typename T>
struct sample_value {};
template<> struct sample_value<char> { static constexpr char val = 122; };
template<> struct sample_value<int64_t> { static constexpr int64_t val = 1l<<62; };
template<> struct sample_value<double> { static constexpr double val = 17324412.552; };
template<> struct sample_value<std::string> { static const std::string val; };
const std::string sample_value<std::string>::val(200, 'a');


TEMPLATE_TEST_CASE("pushpull", "[basic][throughput]", char, double, std::string) {
	const std::size_t max_nchan = 128, chunk_size = 128;
	const std::size_t param_nchan[] = {1, max_nchan};
	const std::size_t param_inlets[] = {0, 1, 10};

	const TestType data[max_nchan * chunk_size] = {sample_value<TestType>::val};

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
			std::string suffix(std::to_string(nchan) + "_inlets_" + std::to_string(n_inlets));

			BENCHMARK("push_sample_nchan_" + suffix) {
				for (size_t s = 0; s < chunk_size; s++) out.push_sample(data);
			};

			BENCHMARK("push_chunk_nchan_" + suffix) {
				out.push_chunk_multiplexed(data, chunk_size);
			};
			for (auto &inlet : inlet_list) inlet.flush();
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
