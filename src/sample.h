#ifndef SAMPLE_H
#define SAMPLE_H
#include "cast.h"
#include "common.h"
#include "forward.h"
#include <atomic>
#include <boost/endian/conversion.hpp>
#include <boost/serialization/split_member.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <iosfwd>

#ifndef BOOST_BYTE_ORDER
#if BOOST_ENDIAN_BIG_BYTE
const int BOOST_BYTE_ORDER = 4321;
#elif BOOST_ENDIAN_LITTLE_BYTE
const int BOOST_BYTE_ORDER = 1234;
#elif BOOST_ENDIAN_LITTLE_WORD
const int BOOST_BYTE_ORDER = 2134;
#endif
#endif

// Boost.Endian has no functions to reverse floats, so we pretend they're ints of the same size.
template <typename T> inline void endian_reverse_inplace(T &t) {
	lslboost::endian::endian_reverse_inplace(t);
}
template <> inline void endian_reverse_inplace(double &t) {
	endian_reverse_inplace(*((uint64_t *)&t));
}
template <> inline void endian_reverse_inplace(float &t) {
	endian_reverse_inplace(*((uint32_t *)&t));
}

namespace lsl {
// assert that the target CPU can represent the double-precision timestamp format required by LSL
static_assert(sizeof(double) == 8, "Target arch has unexpected double size (!=8)");

// constants used in the network protocol
const uint8_t TAG_DEDUCED_TIMESTAMP = 1;
const uint8_t TAG_TRANSMITTED_TIMESTAMP = 2;

/// channel format properties
const uint8_t format_sizes[] = {0, sizeof(float), sizeof(double), sizeof(std::string), sizeof(int32_t),
	sizeof(int16_t), sizeof(int8_t), 8};
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

	/// Create a new sample whose memory is not managed by the factory.
	static sample *new_sample_unmanaged(
		lsl_channel_format_t fmt, uint32_t num_chans, double timestamp, bool pushthrough);

private:
	/// ensure that a given value is a multiple of some base, round up if necessary
	static uint32_t ensure_multiple(uint32_t v, unsigned base) {
		return (v % base) ? v - (v % base) + base : v;
	}

	/// Pop a sample from the freelist (multi-producer/single-consumer queue by Dmitry Vjukov)
	sample *pop_freelist();

	friend class sample;
	/// the channel format to construct samples with
	lsl_channel_format_t fmt_;
	/// the number of channels to construct samples with
	const uint32_t num_chans_;
	/// size of a sample, in bytes
	const uint32_t sample_size_;
	/// size of the allocated storage, in bytes
	uint32_t storage_size_;
	/// a slab of storage for pre-allocated samples
	char *storage_;
	/// a sentinel element for our freelist
	sample *sentinel_;
	/// head of the freelist
	std::atomic<sample *> head_;
	/// tail of the freelist
	sample *tail_;
};

/**
 * The sample data type.
 * Used to represent samples across the library's various buffers and can be serialized (e.g., over
 * the network).
 */
class sample {
public:
	friend class factory;
	/// time-stamp of the sample
	double timestamp;
	/// whether the sample shall be buffered or pushed through
	bool pushthrough;

private:
	/// the channel format
	lsl_channel_format_t format_;
	/// number of channels
	uint32_t num_channels_;
	/// reference count used by sample_p
	std::atomic<int> refcount_;
	/// linked list of samples, for use in a freelist
	std::atomic<sample *> next_;
	/// the factory used to reclaim this sample, if any
	factory *factory_;
	/// the data payload begins here
	alignas(8) char data_;

public:
	// === Construction ===

	/// Destructor for a sample.
	~sample() {
		if (format_ == cft_string)
			for (std::string *p = (std::string *)&data_, *e = p + num_channels_; p < e;
				 (p++)->~basic_string<char>())
				;
	}

	/// Delete a sample.
	void operator delete(void *x) {
		// delete the underlying memory only if it wasn't allocated in the factory's storage area
		sample *s = (sample *)x;
		if (s && !(s->factory_ &&
					 (((char *)s) >= s->factory_->storage_ &&
						 ((char *)s) <= s->factory_->storage_ + s->factory_->storage_size_)))
			delete[](char *) x;
	}

	/// Test for equality with another sample.
	bool operator==(const sample &rhs) const noexcept;

	std::size_t datasize() const { return format_sizes[format_] * num_channels_; }

	// === type-safe accessors ===

	/// Assign an array of numeric values (with type conversions).
	template <class T> sample &assign_typed(const T *s) {
		if ((sizeof(T) == format_sizes[format_]) &&
			((std::is_integral<T>::value && format_integral[format_]) ||
				(std::is_floating_point<T>::value && format_float[format_]))) {
			memcpy(&data_, s, datasize());
		} else {
			switch (format_) {
			case cft_float32:
				for (float *p = (float *)&data_, *e = p + num_channels_; p < e; *p++ = (float)*s++)
					;
				break;
			case cft_double64:
				for (double *p = (double *)&data_, *e = p + num_channels_; p < e;
					 *p++ = (double)*s++)
					;
				break;
			case cft_int8:
				for (int8_t *p = (int8_t *)&data_, *e = p + num_channels_; p < e;
					 *p++ = (int8_t)*s++)
					;
				break;
			case cft_int16:
				for (int16_t *p = (int16_t *)&data_, *e = p + num_channels_; p < e;
					 *p++ = (int16_t)*s++)
					;
				break;
			case cft_int32:
				for (int32_t *p = (int32_t *)&data_, *e = p + num_channels_; p < e;
					 *p++ = (int32_t)*s++)
					;
				break;
#ifndef BOOST_NO_INT64_T
			case cft_int64:
				for (int64_t *p = (int64_t *)&data_, *e = p + num_channels_; p < e;
					 *p++ = (int64_t)*s++)
					;
				break;
#endif
			case cft_string:
				for (std::string *p = (std::string *)&data_, *e = p + num_channels_; p < e;
					 *p++ = to_string(*s++))
					;
				break;
			default: throw std::invalid_argument("Unsupported channel format.");
			}
		}
		return *this;
	}

	/// Retrieve an array of numeric values (with type conversions).
	template <class T> sample &retrieve_typed(T *d) {
		if ((sizeof(T) == format_sizes[format_]) &&
			((std::is_integral<T>::value && format_integral[format_]) ||
				(std::is_floating_point<T>::value && format_float[format_]))) {
			memcpy(d, &data_, datasize());
		} else {
			switch (format_) {
			case cft_float32:
				for (float *p = (float *)&data_, *e = p + num_channels_; p < e; *d++ = (T)*p++)
					;
				break;
			case cft_double64:
				for (double *p = (double *)&data_, *e = p + num_channels_; p < e; *d++ = (T)*p++)
					;
				break;
			case cft_int8:
				for (int8_t *p = (int8_t *)&data_, *e = p + num_channels_; p < e; *d++ = (T)*p++)
					;
				break;
			case cft_int16:
				for (int16_t *p = (int16_t *)&data_, *e = p + num_channels_; p < e; *d++ = (T)*p++)
					;
				break;
			case cft_int32:
				for (int32_t *p = (int32_t *)&data_, *e = p + num_channels_; p < e; *d++ = (T)*p++)
					;
				break;
#ifndef BOOST_NO_INT64_T
			case cft_int64:
				for (int64_t *p = (int64_t *)&data_, *e = p + num_channels_; p < e; *d++ = (T)*p++)
					;
				break;
#endif
			case cft_string:
				for (std::string *p = (std::string *)&data_, *e = p + num_channels_; p < e;
					 *d++ = from_string<T>(*p++))
					;
				break;
			default: throw std::invalid_argument("Unsupported channel format.");
			}
		}
		return *this;
	}

	/// Assign an array of string values to the sample.
	sample &assign_typed(const std::string *s);

	/// Retrieve an array of string values from the sample.
	sample &retrieve_typed(std::string *d);

	// === untyped accessors ===

	/// Assign numeric data to the sample.
	sample &assign_untyped(const void *newdata) {
		if (format_ != cft_string)
			memcpy(&data_, newdata, datasize());
		else
			throw std::invalid_argument("Cannot assign untyped data to a string-formatted sample.");
		return *this;
	}

	/// Retrieve numeric data from the sample.
	sample &retrieve_untyped(void *newdata) {
		if (format_ != cft_string)
			memcpy(newdata, &data_, datasize());
		else
			throw std::invalid_argument(
				"Cannot retrieve untyped data from a string-formatted sample.");
		return *this;
	}

	// === serialization functions ===

	/// Helper function to save raw binary data to a stream buffer.
	static void save_raw(std::streambuf &sb, const void *address, std::size_t count);

	/// Helper function to load raw binary data from a stream buffer.
	static void load_raw(std::streambuf &sb, void *address, std::size_t count);

	/// Save a value to a stream buffer with correct endian treatment.
	template <typename T> static void save_value(std::streambuf &sb, T v, int use_byte_order) {
		if (use_byte_order != BOOST_BYTE_ORDER) endian_reverse_inplace(v);
		save_raw(sb, &v, sizeof(T));
	}

	/// Load a value from a stream buffer with correct endian treatment.
	template <typename T> static void load_value(std::streambuf &sb, T &v, int use_byte_order) {
		load_raw(sb, &v, sizeof(v));
		if (use_byte_order != BOOST_BYTE_ORDER) endian_reverse_inplace(v);
	}

	/// Load a value from a stream buffer; specialization of the above.
	void load_value(std::streambuf &sb, uint8_t &v, int) {
		load_raw(sb, &v, sizeof(v));
	}

	/// Serialize a sample to a stream buffer (protocol 1.10).
	void save_streambuf(std::streambuf &sb, int protocol_version, int use_byte_order,
		void *scratchpad = nullptr) const;

	/// Deserialize a sample from a stream buffer (protocol 1.10).
	void load_streambuf(
		std::streambuf &sb, int protocol_version, int use_byte_order, bool suppress_subnormals);

	/// Convert the endianness of channel data in-place.
	void convert_endian(void *data) const {
		switch (format_sizes[format_]) {
		case 1: break;
		case sizeof(int16_t):
			for (int16_t *p = (int16_t *)data, *e = p + num_channels_; p < e;
				 endian_reverse_inplace(*p++))
				;
			break;
		case sizeof(int32_t):
			for (int32_t *p = (int32_t *)data, *e = p + num_channels_; p < e;
				 endian_reverse_inplace(*p++))
				;
			break;
#ifndef BOOST_NO_INT64_T
		case sizeof(int64_t):
			for (int64_t *p = (int64_t *)data, *e = p + num_channels_; p < e;
				 endian_reverse_inplace(*p++))
				;
			break;
#else
		case sizeof(double):
			for (double *p = (double *)data, *e = p + num_channels_; p < e;
				 endian_reverse_inplace(*p++))
				;
			break;
#endif
		default: throw std::runtime_error("Unsupported channel format for endian conversion.");
		}
	}
	/// Serialize a sample into a portable archive (protocol 1.00).
	void save(eos::portable_oarchive &ar, const uint32_t archive_version) const;

	/// Deserialize a sample from a portable archive (protocol 1.00).
	void load(eos::portable_iarchive &ar, const uint32_t archive_version);

	/// Serialize (read/write) the channel data.
	template <class Archive> void serialize_channels(Archive &ar, const uint32_t archive_version);

	BOOST_SERIALIZATION_SPLIT_MEMBER()

	/// Assign a test pattern to the sample (for protocol validation)
	sample &assign_test_pattern(int offset = 1);

private:
	/// Construct a new sample for a given channel format/count combination.
	sample(lsl_channel_format_t fmt, uint32_t num_channels, factory *fact)
		: format_(fmt), num_channels_(num_channels), refcount_(0), next_(nullptr), factory_(fact) {
		if (format_ == cft_string)
			for (std::string *p = (std::string *)&data_, *e = p + num_channels_; p < e;
				 new (p++) std::string())
				;
	}

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
};

} // namespace lsl

#endif
