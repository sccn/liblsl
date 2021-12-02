#pragma once

#include <random>
#include <string>

struct UUID {
	uint8_t data[16]{0};

	void set_version(uint8_t version = 4) {
		data[6] = (data[6] & 0x0f) | static_cast<uint8_t>(version << 4);
	}

	void set_variant2() { data[8] = (data[8] & 0x3f) | 0x80; }

	/// return the UUID formatted as RFC 4122 UUID
	std::string to_string() const {
		std::string out(36, '0');
		auto pos = out.begin();
		const uint8_t *curbyte = data;

		/// helper: write `n` hex-formatted bytes as hex to a string, incrementing the positions
		const auto copybytesashex = [&pos, &curbyte](int n) {
			const char tbl[] = "0123456789abcdef";
			for (int i = 0; i < n; ++i) {
				*pos++ = tbl[*curbyte / 16];
				*pos++ = tbl[*curbyte++ & 0x0f];
			}
		};

		copybytesashex(2);

		// 4x "abcd-"
		for (int i = 0; i < 4; ++i) {
			copybytesashex(2);
			*pos++ = '-';
		}
		copybytesashex(6);
		return out;
	}

	/// generate a random UUID4
	static UUID random() {
		UUID uuid;

		// cast data to an array of the RNGs native type, write random bytes directly to it
		std::random_device rng;
		using rand_t = std::random_device::result_type;
		constexpr int rand_elems = sizeof(UUID::data) / sizeof(rand_t);
		auto *data_view = reinterpret_cast<rand_t *>(uuid.data);
		for (int i = 0; i < rand_elems; ++i) data_view[i] = rng();

		uuid.set_version(4);
		uuid.set_variant2();
		return uuid;
	}
};
