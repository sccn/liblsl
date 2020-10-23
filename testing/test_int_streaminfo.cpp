#include "../src/api_config.h"
#include "../src/stream_info_impl.h"
#include "catch.hpp"

TEST_CASE("streaminfo matching via XPath", "[basic][streaminfo][xml]") {
	lsl::stream_info_impl info(
		"streamname", "streamtype", 8, 500, lsl_channel_format_t::cft_string, "sourceid");
	auto channels = info.desc().append_child("channels");
	for (int i = 0; i < 4; ++i)
		channels.append_child("channel")
			.append_child("type")
			.append_child(pugi::node_pcdata)
			.set_value("EEG");
	for (int i = 0; i < 4; ++i)
		channels.append_child("channel")
			.append_child("type")
			.append_child(pugi::node_pcdata)
			.set_value("EOG");

	INFO(info.to_fullinfo_message());
	REQUIRE(info.matches_query("name='streamname'"));
	REQUIRE(info.matches_query("name='streamname' and type='streamtype'"));
	REQUIRE(info.matches_query("channel_count > 5"));
	REQUIRE(info.matches_query("nominal_srate >= 499"));
	REQUIRE(info.matches_query("count(desc/channels/channel[type='EEG'])>3"));

	LOG_F(INFO, "The following warning is harmless and expected");
	REQUIRE(!info.matches_query("in'va'lid"));
	REQUIRE(!info.matches_query("name='othername'"));

#ifdef CATCH_CONFIG_ENABLE_BENCHMARKING
	// Append lots of dummy channels for performance tests
	for (int i = 0; i < 50000; ++i)
		channels.append_child("chn")
			.append_child("type")
			.append_child(pugi::node_pcdata)
			.set_value("foobar");
	for (int i = 0; i < 2000; ++i) {
		channels = channels.append_child("chn");
		channels.append_child(pugi::node_pcdata).set_value("1");
	}

	BENCHMARK("trivial query") {
		return info.matches_query("name='streamname' and type='streamtype'", true);
	};
	BENCHMARK("complicated query") {
		return info.matches_query("count(desc/channels/channel[type='EEG'])>3", true);
	};
	BENCHMARK("Cached query") {
		return info.matches_query("count(desc/channels/channel[type='EEG'])>3", false);
	};

	// test how well the cache copes with lots of different queries
	BENCHMARK("partially cached queries (x1000)") {
		int matches = 0;
		for (int j = 0; j < 1000; ++j)
			matches += info.matches_query(("0<=" + std::to_string(j)).c_str());
		return matches;
	};

#endif
}
