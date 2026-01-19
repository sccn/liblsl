#include "sample.h"
#include "util/endian.hpp"
#include <catch2/catch_all.hpp>
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
	for (const auto &buf : bufs) {
		if (buf.size == 1) {
			// Tag byte - no swap, would just pass through
			CHECK(buf.data[0] == tag);
		} else if (buf.size == sizeof(double)) {
			// Timestamp - swap as double
			size_t offset = swapped_data.size();
			swapped_data.resize(offset + sizeof(double));
			std::memcpy(swapped_data.data() + offset, buf.data, sizeof(double));
			lsl::endian_reverse_inplace(*reinterpret_cast<double *>(swapped_data.data() + offset));

			// Verify it's different from original
			double swapped_ts;
			std::memcpy(&swapped_ts, swapped_data.data() + offset, sizeof(double));
			CHECK(swapped_ts != timestamp);
		} else if (buf.size == sample_bytes) {
			// Sample data - swap each value
			size_t offset = swapped_data.size();
			swapped_data.resize(offset + sample_bytes);
			std::memcpy(swapped_data.data() + offset, buf.data, sample_bytes);
			lsl::sample::convert_endian(swapped_data.data() + offset, num_channels, value_size);

			// Verify values are different from original
			float *swapped_samples = reinterpret_cast<float *>(swapped_data.data() + offset);
			for (uint32_t i = 0; i < num_channels; ++i) {
				CHECK(swapped_samples[i] != sample_data[i]);
			}
		}
	}

	// Now verify that swapping back recovers original values
	// Swap timestamp back
	lsl::endian_reverse_inplace(*reinterpret_cast<double *>(swapped_data.data()));
	double recovered_ts;
	std::memcpy(&recovered_ts, swapped_data.data(), sizeof(double));
	CHECK(recovered_ts == timestamp);

	// Swap samples back
	lsl::sample::convert_endian(swapped_data.data() + sizeof(double), num_channels, value_size);
	float *recovered_samples = reinterpret_cast<float *>(swapped_data.data() + sizeof(double));
	for (uint32_t i = 0; i < num_channels; ++i) {
		CHECK(recovered_samples[i] == sample_data[i]);
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
