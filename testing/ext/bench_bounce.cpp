#include "../common/create_streampair.hpp"
#include "../common/lsltypes.hpp"
#include <catch2/catch_all.hpp>
#include <lsl_cpp.h>

// clazy:excludeall=non-pod-global-static

TEST_CASE("bounce", "[basic][latency]") {
	auto sp = create_streampair(lsl::stream_info("bounce", "Test"));

	float data = .0;
	BENCHMARK("single bounce") {
		sp.out_.push_sample(&data);
		sp.in_.pull_sample(&data, 1.);
	};

	sp.out_.push_sample(&data);
	BENCHMARK("primed bounce") {
		sp.out_.push_sample(&data);
		sp.in_.pull_sample(&data, 1.);
	};
}
