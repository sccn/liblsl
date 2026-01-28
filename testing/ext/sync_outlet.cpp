#include "../common/create_streampair.hpp"
#include "../common/lsltypes.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <lsl_cpp.h>
#include <thread>
#include <vector>

// clazy:excludeall=non-pod-global-static

/// Helper to create a sync outlet + inlet pair
inline Streampair create_sync_streampair(const lsl::stream_info &info) {
	lsl::stream_outlet out(info, 0, 360, transp_sync_blocking);
	auto found_stream_info = lsl::resolve_stream("name", info.name(), 1, 5.0);
	if (found_stream_info.empty()) throw std::runtime_error("sync outlet not found");
	lsl::stream_inlet in(found_stream_info[0]);

	in.open_stream(2);
	out.wait_for_consumers(2);
	return Streampair(std::move(out), std::move(in));
}

TEST_CASE("sync_outlet_basic", "[sync][basic]") {
	const int nchannels = 4;
	const int nsamples = 100;

	auto sp = create_sync_streampair(
		lsl::stream_info("SyncBasic", "Test", nchannels, 100, lsl::cf_float32, "sync_basic"));

	std::vector<float> send_buf(nchannels);
	std::vector<float> recv_buf(nchannels);

	for (int i = 0; i < nsamples; ++i) {
		// Fill with test pattern
		for (int c = 0; c < nchannels; ++c) { send_buf[c] = static_cast<float>(i * nchannels + c); }

		double send_ts = lsl::local_clock();
		sp.out_.push_sample(send_buf, send_ts, true);

		double recv_ts = sp.in_.pull_sample(recv_buf, 2.0);
		REQUIRE(recv_ts != 0.0);

		for (int c = 0; c < nchannels; ++c) {
			CHECK(recv_buf[c] == Catch::Approx(send_buf[c]));
		}
	}
}

TEMPLATE_TEST_CASE(
	"sync_outlet_datatypes", "[sync][datatransfer]", char, int16_t, int32_t, int64_t, float, double) {
	const int nchannels = 2;
	const int nsamples = 32;
	const char *name = SampleType<TestType>::fmt_string();
	auto cf = static_cast<lsl::channel_format_t>(SampleType<TestType>::chan_fmt);

	lsl::stream_info info(std::string("Sync_") + name, "Test", nchannels, 100, cf, "sync_dtype");
	auto sp = create_sync_streampair(info);

	TestType send_buf[nchannels];
	TestType recv_buf[nchannels];

	// Test with shifting bit pattern to exercise all bits
	send_buf[0] = 1;
	for (int i = 0; i < nsamples; ++i) {
		send_buf[1] = static_cast<TestType>(-send_buf[0] + 1);

		sp.out_.push_sample(send_buf, lsl::local_clock(), true);
		double ts = sp.in_.pull_sample(recv_buf, 2.0);
		REQUIRE(ts != 0.0);

		CHECK(recv_buf[0] == Catch::Approx(send_buf[0]));
		CHECK(recv_buf[1] == Catch::Approx(send_buf[1]));

		send_buf[0] = static_cast<TestType>(static_cast<int64_t>(send_buf[0]) << 1);
		if (send_buf[0] == 0) send_buf[0] = 1; // Reset if we shifted out all bits
	}
}

TEST_CASE("sync_outlet_string_rejected", "[sync][validation]") {
	// Sync mode should reject string-format streams
	lsl::stream_info info("SyncString", "Test", 1, 100, lsl::cf_string, "sync_string");

	CHECK_THROWS_AS(
		lsl::stream_outlet(info, 0, 360, transp_sync_blocking), std::invalid_argument);
}

TEST_CASE("sync_outlet_push_chunk", "[sync][chunk]") {
	const int nchannels = 4;
	const int chunk_size = 10;

	auto sp = create_sync_streampair(
		lsl::stream_info("SyncChunk", "Test", nchannels, 100, lsl::cf_float32, "sync_chunk"));

	// Create chunk data (multiplexed: ch0_s0, ch1_s0, ch2_s0, ch3_s0, ch0_s1, ...)
	std::vector<float> chunk(nchannels * chunk_size);
	for (int s = 0; s < chunk_size; ++s) {
		for (int c = 0; c < nchannels; ++c) {
			chunk[s * nchannels + c] = static_cast<float>(s * 100 + c);
		}
	}

	// Push chunk with single timestamp - this uses DEDUCED_TIMESTAMP for samples 2-N
	sp.out_.push_chunk_multiplexed(chunk, lsl::local_clock(), true);

	// Pull and verify all samples
	std::vector<float> recv_buf(nchannels);
	for (int s = 0; s < chunk_size; ++s) {
		double ts = sp.in_.pull_sample(recv_buf, 2.0);
		INFO("Sample " << s << ": ts=" << ts);
		REQUIRE(ts != 0.0);
		for (int c = 0; c < nchannels; ++c) {
			INFO("  ch" << c << ": got=" << recv_buf[c] << " expected=" << chunk[s * nchannels + c]);
			CHECK(recv_buf[c] == Catch::Approx(chunk[s * nchannels + c]));
		}
	}
}

TEST_CASE("sync_outlet_multi_consumer", "[sync][multi]") {
	const int nchannels = 2;
	const int nsamples = 50;

	lsl::stream_info info("SyncMulti", "Test", nchannels, 100, lsl::cf_float32, "sync_multi");
	lsl::stream_outlet out(info, 0, 360, transp_sync_blocking);

	// Resolve and connect two inlets
	auto found = lsl::resolve_stream("name", "SyncMulti", 1, 5.0);
	REQUIRE(!found.empty());

	lsl::stream_inlet in1(found[0]);
	lsl::stream_inlet in2(found[0]);

	in1.open_stream(2);
	in2.open_stream(2);
	out.wait_for_consumers(2);

	// Give sockets time to be handed off to sync handler
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	std::vector<float> send_buf(nchannels);
	std::vector<float> recv_buf1(nchannels);
	std::vector<float> recv_buf2(nchannels);

	for (int i = 0; i < nsamples; ++i) {
		send_buf[0] = static_cast<float>(i);
		send_buf[1] = static_cast<float>(-i);

		out.push_sample(send_buf, lsl::local_clock(), true);

		double ts1 = in1.pull_sample(recv_buf1, 2.0);
		double ts2 = in2.pull_sample(recv_buf2, 2.0);

		REQUIRE(ts1 != 0.0);
		REQUIRE(ts2 != 0.0);

		CHECK(recv_buf1[0] == Catch::Approx(send_buf[0]));
		CHECK(recv_buf1[1] == Catch::Approx(send_buf[1]));
		CHECK(recv_buf2[0] == Catch::Approx(send_buf[0]));
		CHECK(recv_buf2[1] == Catch::Approx(send_buf[1]));
	}
}

TEST_CASE("sync_outlet_consumer_disconnect", "[sync][disconnect]") {
	const int nchannels = 2;

	lsl::stream_info info(
		"SyncDisconnect", "Test", nchannels, 100, lsl::cf_float32, "sync_disconnect");
	lsl::stream_outlet out(info, 0, 360, transp_sync_blocking);

	{
		// Create inlet in a scope so it gets destroyed
		auto found = lsl::resolve_stream("name", "SyncDisconnect", 1, 5.0);
		REQUIRE(!found.empty());

		lsl::stream_inlet in(found[0]);
		in.open_stream(2);
		out.wait_for_consumers(2);

		// Give socket time to be handed off
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		// Send some samples successfully
		std::vector<float> buf = {1.0f, 2.0f};
		out.push_sample(buf, lsl::local_clock(), true);

		std::vector<float> recv(nchannels);
		CHECK(in.pull_sample(recv, 2.0) != 0.0);
	}
	// Inlet is now destroyed

	// Give time for disconnect to propagate
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// Outlet should handle the disconnected consumer gracefully
	// (no crash, no hang - just returns quickly with no consumers)
	// Note: First write(s) might succeed (OS buffers), so we do multiple writes
	// with delays to ensure the broken pipe error is eventually detected.
	std::vector<float> buf = {3.0f, 4.0f};
	for (int i = 0; i < 5 && out.have_consumers(); ++i) {
		out.push_sample(buf, lsl::local_clock(), true);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	CHECK(!out.have_consumers());
}
