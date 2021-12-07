#ifndef SAMPLE_H
#define SAMPLE_H
#include "common.h"
#include "forward.h"
#include "util/cast.hpp"
#include "util/endian.hpp"
#include <atomic>
#include <cstdint>
#include <iosfwd>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>


namespace lsl {
// assert that the target CPU can represent the double-precision timestamp format required by LSL
static_assert(sizeof(double) == 8, "Target arch has unexpected double size (!=8)");

// constants used in the network protocol
const uint8_t TAG_DEDUCED_TIMESTAMP = 1;
const uint8_t TAG_TRANSMITTED_TIMESTAMP = 2;

/// channel format properties
const uint8_t format_sizes[] = {0, sizeof(float), sizeof(double), sizeof(std::string),
	sizeof(int32_t), sizeof(int16_t), sizeof(int8_t), 8};
const bool format_ieee754[] = {false, std::numeric_limits<float>::is_iec559,
	std::numeric_limits<double>::is_iec559, false, false, false, false, false};
const bool format_subnormal[] = {false,
	std::numeric_limits<float>::has_denorm != std::denorm_absent,
	std::numeric_limits<double>::has_denorm != std::denorm_absent, false, false, false, false,
	false};
const bool format_integral[] = {false, false, false, false, true, true, true, true};
const bool format_float[] = {false, true, true, false, false, false, false, false};

/// A factory to create samples of a given format/size. Must outlive all of its created samples.
class factory {
public:
	/**
	 * Create a new factory and optionally pre-allocate samples.
	 * @param fmt Sample format
	 * @param num_chans nr of channels
	 * @param num_reserve nr of samples to pre-allocate in the storage pool
	 */
	factory(lsl_channel_format_t fmt, uint32_t num_chans, uint32_t num_reserve);

	/// Destroy the factory and delete all of its samples.
	~factory();

	/// Create a new sample with a given timestamp and pushthrough flag.
	/// Only one thread may call this function for a given factory object.
	sample_p new_sample(double timestamp, bool pushthrough);

	/// Reclaim a sample that's no longer used.
	void reclaim_sample(sample *s);

private:
	/// Pop a sample from the freelist (multi-producer/single-consumer queue by Dmitry Vjukov)
	sample *pop_freelist();

	/// Return the address of a pre-allocated sample, #0 is the sentinel value
	sample *sample_by_index(std::size_t idx) const {
		return reinterpret_cast<sample*>(storage_ + idx * sample_size_);
	}

	/// Return the address of the sentinel value
	sample *sentinel() const {
		return sample_by_index(0);
	}

	friend class sample;
	/// the channel format to construct samples with
	const lsl_channel_format_t fmt_;
	/// the number of channels to construct samples with
	const uint32_t num_chans_;
	/// size of a sample, in bytes
	const uint32_t sample_size_;
	/// size of the allocated storage, in bytes
	const uint32_t storage_size_;
	/// a slab of storage for pre-allocated samples
	char *const storage_;
	/// head of the freelist
	std::atomic<sample *> head_;
	/// tail of the freelist
	std::atomic<sample *> tail_;
};

/**
 * The sample data type.
 * Used to represent samples across the library's various buffers and can be serialized (e.g., over
 * the network).
 */
class sample {
public:
	friend class factory;
	/// whether the sample shall be buffered or pushed through
	bool pushthrough{false};

private:
	/// the channel format
	const lsl_channel_format_t format_;
	/// number of channels
	const uint32_t num_channels_;
	/// reference count used by sample_p
	std::atomic<int> refcount_;
	/// linked list of samples, for use in a freelist
	std::atomic<sample *> next_;
	/// the factory used to reclaim this sample
	factory *const factory_;
	/// time-stamp of the sample
	double timestamp_{0.0};
	/// the data payload begins here
	alignas(8) int32_t data_{0};

public:
	// === Construction ===

	/// Destructor for a sample.
	~sample() noexcept;

	double &timestamp() { return timestamp_; }

	/// Delete a sample.
	void operator delete(void *x) noexcept;

	/// Test for equality with another sample.
	bool operator==(const sample &rhs) const noexcept;
	bool operator!=(const sample &rhs) const noexcept { return !(*this == rhs); }

	std::size_t datasize() const { return format_sizes[format_] * static_cast<std::size_t>(num_channels_); }

	uint32_t num_channels() const { return num_channels_; }

	// === type-safe accessors ===

	/// Assign an array of numeric values (with type conversions).
	template <class T> void assign_typed(const T *s);

	/// Retrieve an array of numeric values (with type conversions).
	template <class T> void retrieve_typed(T *d);

	// === untyped accessors ===

	/// Assign numeric data to the sample.
	void assign_untyped(const void *newdata);

	/// Retrieve numeric data from the sample.
	void retrieve_untyped(void *newdata);

	// === serialization functions ===

	/// Serialize a sample to a stream buffer (protocol 1.10).
	void save_streambuf(std::streambuf &sb, int protocol_version, bool reverse_byte_order,
		void *scratchpad = nullptr) const;

	/// Deserialize a sample from a stream buffer (protocol 1.10).
	void load_streambuf(std::streambuf &sb, int protocol_version, bool reverse_byte_order,
		bool suppress_subnormals);

	/// Convert the endianness of channel data in-place.
	static void convert_endian(void *data, uint32_t n, uint32_t width);

	/// Serialize a sample into a portable archive (protocol 1.00).
	void serialize(eos::portable_oarchive &ar, uint32_t archive_version) const;

	/// Deserialize a sample from a portable archive (protocol 1.00).
	void serialize(eos::portable_iarchive &ar, uint32_t archive_version);

	/// Serialize (read/write) the channel data.
	template <class Archive> void serialize_channels(Archive &ar, uint32_t archive_version);

	/// Assign a test pattern to the sample (for protocol validation)
	sample &assign_test_pattern(int offset = 1);

private:
	/// Construct a new sample for a given channel format/count combination.
	sample(lsl_channel_format_t fmt, uint32_t num_channels, factory *fact);

	/// Increment ref count.
	friend void intrusive_ptr_add_ref(sample *s) {
		s->refcount_.fetch_add(1, std::memory_order_relaxed);
	}

	/// Decrement ref count and reclaim if unreferenced.
	friend void intrusive_ptr_release(sample *s) {
		if (s->refcount_.fetch_sub(1, std::memory_order_release) == 1) {
			std::atomic_thread_fence(std::memory_order_acquire);
			s->factory_->reclaim_sample(s);
		}
	}

	friend void *iterhelper(sample &s) noexcept { return reinterpret_cast<void *>(&s.data_); }
	friend const void *iterhelper(const sample &s) noexcept {
		return reinterpret_cast<const void *>(&s.data_);
	}

	template <typename T, typename U> void conv_from(const U *src);
	template <typename T, typename U> void conv_into(U *dst);
};

} // namespace lsl

#endif
