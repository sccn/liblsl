#ifndef SYNC_SERIALIZATION_H
#define SYNC_SERIALIZATION_H

#include "sample.h"
#include <asio/buffer.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace lsl {

/**
 * Byte-swap a synchronous gather-write buffer sequence for a reverse-endianness client.
 *
 * The input is the on-the-wire layout produced by the sync outlet, one group per sample:
 *   [tag:1] ( [timestamp:8] iff tag == TAG_TRANSMITTED_TIMESTAMP ) [sample:sample_bytes]
 *
 * The sequence is walked by tag rather than by guessing from buffer sizes, so it stays
 * correct even when a sample happens to be 8 bytes (e.g. 2x int32 or 4x int16), which a
 * size-based classifier would mistake for a timestamp. Tag bytes pass through unchanged,
 * timestamps are reversed as a single 8-byte value, and sample payloads are reversed per
 * channel value (@p value_size bytes each; @p value_size == 1 is a no-op).
 *
 * Swapped bytes are written into @p storage, which is cleared and reserved to the exact
 * required size up front so that the returned const_buffers referencing it remain valid
 * for the lifetime of the returned vector (a growing vector would otherwise reallocate and
 * dangle earlier pointers).
 *
 * @param bufs         Gather buffers for one or more samples, in the layout described above.
 * @param storage      Scratch storage that backs the swapped buffers; must outlive the result.
 * @param sample_bytes Total bytes per sample (channel_count * value_size).
 * @param value_size   Bytes per channel value (1, 2, 4 or 8).
 * @param num_channels Number of channels per sample.
 * @return const_buffers describing the byte-swapped stream; tag buffers alias @p bufs, the
 *         rest point into @p storage.
 */
inline std::vector<asio::const_buffer> sync_swap_buffers(
	const std::vector<asio::const_buffer> &bufs, std::vector<char> &storage,
	std::size_t sample_bytes, std::size_t value_size, uint32_t num_channels) {
	(void)sample_bytes; // implied by num_channels * value_size; kept for call-site clarity

	// Reserve the exact swapped byte count (everything except the 1-byte tags) before
	// appending so that storage never reallocates and the returned pointers stay valid.
	std::size_t needed = 0;
	for (const auto &b : bufs)
		if (b.size() != 1) needed += b.size();
	storage.clear();
	storage.reserve(needed);

	std::vector<asio::const_buffer> result;
	result.reserve(bufs.size());

	const auto swap_into = [&](const asio::const_buffer &buf, std::size_t width) {
		const std::size_t offset = storage.size();
		const std::size_t n = buf.size();
		storage.resize(offset + n); // within reserved capacity -> no reallocation
		std::memcpy(storage.data() + offset, buf.data(), n);
		sample::convert_endian(storage.data() + offset, static_cast<uint32_t>(n / width),
			static_cast<uint32_t>(width));
		result.push_back(asio::const_buffer(storage.data() + offset, n));
	};

	std::size_t i = 0;
	while (i < bufs.size()) {
		// Each sample group starts with a 1-byte tag, which is never byte-swapped.
		const asio::const_buffer &tagbuf = bufs[i];
		const uint8_t tag = *static_cast<const uint8_t *>(tagbuf.data());
		result.push_back(tagbuf);
		++i;
		// A transmitted-timestamp tag is followed by an 8-byte timestamp value.
		if (tag == TAG_TRANSMITTED_TIMESTAMP && i < bufs.size()) {
			swap_into(bufs[i], sizeof(double));
			++i;
		}
		// Then the fixed-size sample payload, swapped per channel value.
		if (i < bufs.size()) {
			swap_into(bufs[i], value_size);
			++i;
		}
	}
	(void)num_channels;
	return result;
}

} // namespace lsl

#endif // SYNC_SERIALIZATION_H
