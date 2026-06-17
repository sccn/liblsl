#include "sample.h"
#include "sync_serialization.h"
#include "util/endian.hpp"
#include <algorithm>
#include <asio/buffer.hpp>
#include <catch2/catch_all.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

// clazy:excludeall=non-pod-global-static

// Test the byte-swapping logic used by sync_write_handler

TEST_CASE("endian_reverse_inplace", "[sync][endian]") {
	SECTION("16-bit") {
		int16_t val = 0x0102;
		lsl::endian_reverse_inplace(val);
		CHECK(val == 0x0201);
	}

	SECTION("32-bit") {
		int32_t val = 0x01020304;
		lsl::endian_reverse_inplace(val);
		CHECK(val == 0x04030201);
	}

	SECTION("64-bit") {
		int64_t val = 0x0102030405060708LL;
		lsl::endian_reverse_inplace(val);
		CHECK(val == 0x0807060504030201LL);
	}

	SECTION("float") {
		// Test that float byte-swap is reversible
		float original = 3.14159f;
		float val = original;
		lsl::endian_reverse_inplace(val);
		CHECK(val != original); // Should be different after swap
		lsl::endian_reverse_inplace(val);
		CHECK(val == original); // Should be same after double-swap
	}

	SECTION("double") {
		double original = 3.141592653589793;
		double val = original;
		lsl::endian_reverse_inplace(val);
		CHECK(val != original);
		lsl::endian_reverse_inplace(val);
		CHECK(val == original);
	}
}

TEST_CASE("sample::convert_endian", "[sync][endian]") {
	SECTION("int16 array") {
		int16_t data[] = {0x0102, 0x0304, 0x0506, 0x0708};
		lsl::sample::convert_endian(data, 4, sizeof(int16_t));
		CHECK(data[0] == 0x0201);
		CHECK(data[1] == 0x0403);
		CHECK(data[2] == 0x0605);
		CHECK(data[3] == 0x0807);
	}

	SECTION("int32 array") {
		int32_t data[] = {0x01020304, 0x05060708};
		lsl::sample::convert_endian(data, 2, sizeof(int32_t));
		CHECK(data[0] == 0x04030201);
		CHECK(data[1] == 0x08070605);
	}

	SECTION("float array") {
		float original[] = {1.0f, 2.0f, 3.0f, 4.0f};
		float data[4];
		std::memcpy(data, original, sizeof(data));

		lsl::sample::convert_endian(data, 4, sizeof(float));
		// After swap, values should be different (garbage floats)
		for (int i = 0; i < 4; ++i) { CHECK(data[i] != original[i]); }

		// Swap back, values should match original
		lsl::sample::convert_endian(data, 4, sizeof(float));
		for (int i = 0; i < 4; ++i) { CHECK(data[i] == original[i]); }
	}

	SECTION("double array") {
		double original[] = {1.0, 2.0, 3.0, 4.0};
		double data[4];
		std::memcpy(data, original, sizeof(data));

		lsl::sample::convert_endian(data, 4, sizeof(double));
		for (int i = 0; i < 4; ++i) { CHECK(data[i] != original[i]); }

		lsl::sample::convert_endian(data, 4, sizeof(double));
		for (int i = 0; i < 4; ++i) { CHECK(data[i] == original[i]); }
	}

	SECTION("1-byte no-op") {
		// 1-byte values should not be modified
		char data[] = {0x01, 0x02, 0x03, 0x04};
		char original[] = {0x01, 0x02, 0x03, 0x04};
		lsl::sample::convert_endian(data, 4, 1);
		CHECK(std::memcmp(data, original, 4) == 0);
	}
}

TEST_CASE("sync buffer swap simulation", "[sync][endian]") {
	// Simulate the buffer structure that sync_write_handler processes:
	// [tag:1][timestamp:8][sample:N] repeated
	// This tests the logic without needing actual sockets

	const uint32_t num_channels = 4;
	const uint32_t value_size = sizeof(float);
	const std::size_t sample_bytes = num_channels * value_size;

	// Create test buffers mimicking the sync path structure
	struct TestBuffer {
		const char *data;
		std::size_t size;
	};

	// Tag byte
	char tag = 0x02; // TAG_TRANSMITTED

	// Timestamp
	double timestamp = 1234567890.123456;
	double ts_copy = timestamp;

	// Sample data (4 floats)
	float sample_data[] = {1.0f, 2.0f, 3.0f, 4.0f};
	float sample_copy[4];
	std::memcpy(sample_copy, sample_data, sizeof(sample_copy));

	std::vector<TestBuffer> bufs = {{&tag, 1},
		{reinterpret_cast<const char *>(&ts_copy), sizeof(double)},
		{reinterpret_cast<const char *>(sample_copy), sample_bytes}};

	// Simulate swap_buffers logic
	std::vector<char> swapped_data;

	// Reverse each width-sized value in a byte range in place. Matches the production
	// sync_swap_buffers() approach: pure byte operations, so it stays valid even though the
	// std::vector<char> backing store may be unaligned for the value type (typed dereferences
	// would be undefined behavior on strict-alignment targets).
	const auto swap_bytes = [](char *p, std::size_t n, std::size_t width) {
		for (char *end = p + n; p < end; p += width) std::reverse(p, p + width);
	};
	// Read a value out of the byte store via an aligned local (no typed dereference of store).
	const auto read_float = [&](std::size_t offset) {
		float v;
		std::memcpy(&v, swapped_data.data() + offset, sizeof(float));
		return v;
	};
	for (const auto &buf : bufs) {
		if (buf.size == 1) {
			// Tag byte - no swap, would just pass through
			CHECK(buf.data[0] == tag);
		} else if (buf.size == sizeof(double)) {
			// Timestamp - swap as a single 8-byte value
			size_t offset = swapped_data.size();
			swapped_data.resize(offset + sizeof(double));
			std::memcpy(swapped_data.data() + offset, buf.data, sizeof(double));
			swap_bytes(swapped_data.data() + offset, sizeof(double), sizeof(double));

			// Verify it's different from original
			double swapped_ts;
			std::memcpy(&swapped_ts, swapped_data.data() + offset, sizeof(double));
			CHECK(swapped_ts != timestamp);
		} else if (buf.size == sample_bytes) {
			// Sample data - swap each value
			size_t offset = swapped_data.size();
			swapped_data.resize(offset + sample_bytes);
			std::memcpy(swapped_data.data() + offset, buf.data, sample_bytes);
			swap_bytes(swapped_data.data() + offset, sample_bytes, value_size);

			// Verify values are different from original
			for (uint32_t i = 0; i < num_channels; ++i) {
				CHECK(read_float(offset + i * value_size) != sample_data[i]);
			}
		}
	}

	// Now verify that swapping back recovers original values
	// Swap timestamp back
	swap_bytes(swapped_data.data(), sizeof(double), sizeof(double));
	double recovered_ts;
	std::memcpy(&recovered_ts, swapped_data.data(), sizeof(double));
	CHECK(recovered_ts == timestamp);

	// Swap samples back
	swap_bytes(swapped_data.data() + sizeof(double), sample_bytes, value_size);
	for (uint32_t i = 0; i < num_channels; ++i) {
		CHECK(read_float(sizeof(double) + i * value_size) == sample_data[i]);
	}
}

TEST_CASE("can_convert_endian", "[sync][endian]") {
	// 1-byte values are always convertible
	CHECK(lsl::can_convert_endian(lsl::LSL_LITTLE_ENDIAN, 1));
	CHECK(lsl::can_convert_endian(lsl::LSL_BIG_ENDIAN, 1));

	// Standard endianness should be convertible for multi-byte values
	CHECK(lsl::can_convert_endian(lsl::LSL_LITTLE_ENDIAN, 2));
	CHECK(lsl::can_convert_endian(lsl::LSL_LITTLE_ENDIAN, 4));
	CHECK(lsl::can_convert_endian(lsl::LSL_LITTLE_ENDIAN, 8));
	CHECK(lsl::can_convert_endian(lsl::LSL_BIG_ENDIAN, 2));
	CHECK(lsl::can_convert_endian(lsl::LSL_BIG_ENDIAN, 4));
	CHECK(lsl::can_convert_endian(lsl::LSL_BIG_ENDIAN, 8));

	// Exotic endianness should not be convertible for multi-byte values
	CHECK_FALSE(lsl::can_convert_endian(lsl::LSL_PORTABLE_ENDIAN, 2));
	CHECK_FALSE(lsl::can_convert_endian(lsl::LSL_LITTLE_ENDIAN_BUT_BIG_FLOAT, 4));
	CHECK_FALSE(lsl::can_convert_endian(lsl::LSL_BIG_ENDIAN_BUT_LITTLE_FLOAT, 4));
	CHECK_FALSE(lsl::can_convert_endian(lsl::LSL_PDP11, 2));
}

// Exercise the real lsl::sync_swap_buffers on the wire layout the sync outlet produces:
//   [tag:1] ([ts:8] iff transmitted) [sample:sample_bytes]  repeated, first sample transmitted.
// This is the cross-endian path that no integration test reaches on a little-endian CI host.
template <class T> static void check_sync_swap_roundtrip(uint32_t nchan, int nsamples) {
	const std::size_t value_size = sizeof(T);
	const std::size_t sample_bytes = nchan * value_size;
	const double ts0 = 1234.5678;

	// Build the host-endian wire bytes into one stable backing buffer, remembering the
	// (offset, size) of each gather buffer so we can build const_buffers once it is final.
	std::vector<char> backing;
	struct Seg {
		std::size_t off, size;
	};
	std::vector<Seg> segs;
	auto append = [&](const void *p, std::size_t n) {
		const std::size_t off = backing.size();
		const char *cp = static_cast<const char *>(p);
		backing.insert(backing.end(), cp, cp + n);
		segs.push_back({off, n});
	};

	std::vector<std::vector<T>> values(nsamples, std::vector<T>(nchan));
	for (int s = 0; s < nsamples; ++s) {
		const uint8_t tag = (s == 0) ? lsl::TAG_TRANSMITTED_TIMESTAMP : lsl::TAG_DEDUCED_TIMESTAMP;
		append(&tag, 1);
		if (s == 0) append(&ts0, sizeof(double));
		for (uint32_t c = 0; c < nchan; ++c)
			values[s][c] = static_cast<T>((s + 1) * 1000 + c * 7 + 1);
		append(values[s].data(), sample_bytes);
	}

	std::vector<asio::const_buffer> bufs;
	for (const auto &sg : segs) bufs.push_back(asio::const_buffer(backing.data() + sg.off, sg.size));

	std::vector<char> storage;
	auto out = lsl::sync_swap_buffers(bufs, storage, sample_bytes, value_size, nchan);
	REQUIRE(out.size() == bufs.size());

	std::size_t bi = 0;
	for (int s = 0; s < nsamples; ++s) {
		// tag byte passes through unchanged
		REQUIRE(out[bi].size() == 1);
		CHECK(*static_cast<const uint8_t *>(out[bi].data()) ==
			  (s == 0 ? lsl::TAG_TRANSMITTED_TIMESTAMP : lsl::TAG_DEDUCED_TIMESTAMP));
		++bi;
		// transmitted timestamp is reversed as a single 8-byte value
		if (s == 0) {
			REQUIRE(out[bi].size() == sizeof(double));
			double exp = ts0;
			lsl::endian_reverse_inplace(exp);
			CHECK(std::memcmp(out[bi].data(), &exp, sizeof(double)) == 0);
			++bi;
		}
		// sample payload is reversed PER CHANNEL VALUE, not as one block — this is what the
		// old size-based classifier got wrong when sample_bytes happened to equal 8.
		REQUIRE(out[bi].size() == sample_bytes);
		for (uint32_t c = 0; c < nchan; ++c) {
			T exp = values[s][c];
			lsl::endian_reverse_inplace(exp);
			const char *got = static_cast<const char *>(out[bi].data()) + c * value_size;
			INFO("sample " << s << " channel " << c);
			CHECK(std::memcmp(got, &exp, value_size) == 0);
		}
		++bi;
	}
}

TEST_CASE("sync_swap_buffers per-value swap and pointer stability", "[sync][endian]") {
	// 2x int32 and 4x int16 both make sample_bytes == 8 (== sizeof(double)); the swap must
	// still reverse each channel value independently rather than as one 8-byte timestamp.
	SECTION("2x int32 (8-byte sample collides with timestamp size)") {
		check_sync_swap_roundtrip<int32_t>(2, 16);
	}
	SECTION("4x int16 (8-byte sample)") { check_sync_swap_roundtrip<int16_t>(4, 16); }
	// Genuine 8-byte values must keep working (reversed as one unit).
	SECTION("1x double (8-byte value)") { check_sync_swap_roundtrip<double>(1, 16); }
	SECTION("1x int64 (8-byte value)") { check_sync_swap_roundtrip<int64_t>(1, 16); }
	// Non-colliding sizes.
	SECTION("3x float") { check_sync_swap_roundtrip<float>(3, 16); }
	SECTION("8x int16") { check_sync_swap_roundtrip<int16_t>(8, 16); }
}
