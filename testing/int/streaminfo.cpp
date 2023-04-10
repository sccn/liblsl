#include "../src/api_config.h"
#include "../src/stream_info_impl.h"
#include <cctype>
#include <loguru.hpp>
// include loguru before catch
#include <catch2/catch_all.hpp>

// clazy:excludeall=non-pod-global-static

template<typename T, const std::size_t N>
bool contains(const T(&valid)[N], const T target) {
	for(T e: valid)
		if(e==target) return true;
	return false;
}

TEST_CASE("uid", "[basic][streaminfo]") {
	lsl::stream_info_impl info;
	const std::string uid = info.reset_uid();
	INFO(uid);
	REQUIRE(uid.length() == 36);
	for(auto i=0u; i<uid.size(); ++i)
		if(i==8||i==13||i==18||i==23) REQUIRE(uid[i] == '-');
		else REQUIRE(std::isxdigit(uid[i]));
	const char version = uid[14];
	REQUIRE(contains("1345", version));

	const char variant = uid[19];
	REQUIRE(contains("89abAB", variant));
	REQUIRE(uid != info.reset_uid());
}

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
