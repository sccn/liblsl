#include <algorithm>
#define BOOST_MATH_DISABLE_STD_FPCLASSIFY
#include "sample.h"
#include "common.h"
#include "portable_archive/portable_iarchive.hpp"
#include "portable_archive/portable_oarchive.hpp"
#include "util/cast.hpp"
#include <boost/endian/conversion.hpp>

using namespace lsl;
using lslboost::endian::endian_reverse_inplace;

#ifdef _MSC_VER
#pragma warning(suppress : 4291)
#endif

/// range-for helper class for `values()`
template <typename T> class dataiter {
	T *begin_, *end_;

public:
	dataiter(void *begin, std::size_t n) : begin_(reinterpret_cast<T *>(begin)), end_(begin_ + n) {}
	dataiter(const void *begin, std::size_t n)
		: begin_(reinterpret_cast<const T *>(begin)), end_(begin_ + n) {}

	// called from `f(auto val: dataiter)`
	T *begin() const noexcept { return begin_; }
	T *end() const noexcept { return end_; }
};

/// range-for helper. Sample usage: `for(auto &val: samplevalues<int32_t>()) val = 2;`
template <typename T> inline dataiter<T> samplevals(sample &s) noexcept {
	return dataiter<T>(iterhelper(s), s.num_channels());
}

template <typename T> inline dataiter<const T> samplevals(const sample &s) noexcept {
	return dataiter<const T>(iterhelper(s), s.num_channels());
}

/// Copy an array, converting between LSL types if needed
template <typename T, typename U>
inline void copyconvert_array(const T *src, U *dst, std::size_t n) noexcept {
	for (const T *end = src + n; src < end;)
		*dst++ = static_cast<U>(*src++); // NOLINT(bugprone-signed-char-misuse)
}

/// Copy an array, special case: source and destination have the same type
template <typename T> inline void copyconvert_array(const T *src, T *dst, std::size_t n) noexcept {
	memcpy(dst, src, n * sizeof(T));
}

/// Copy an array, special case: destination is a string array
template <typename T> inline void copyconvert_array(const T *src, std::string *dst, std::size_t n) {
	for (const T *end = src + n; src < end;) *dst++ = lsl::to_string(*src++);
}

/// Copy an array, special case: source is a string array
template <typename U> inline void copyconvert_array(const std::string *src, U *dst, std::size_t n) {
	for (const auto *end = src + n; src < end;) *dst++ = lsl::from_string<U>(*src++);
}

/// Copy an array, special case: both arrays are string arrays
inline void copyconvert_array(const std::string *src, std::string *dst, std::size_t n) {
	std::copy_n(src, n, dst);
}

template <typename T, typename U> void lsl::sample::conv_from(const U *src) {
	copyconvert_array(src, reinterpret_cast<T *>(&data_), num_channels_);
}

template <typename T, typename U> void lsl::sample::conv_into(U *dst) {
	copyconvert_array(reinterpret_cast<const T *>(&data_), dst, num_channels_);
}

void sample::operator delete(void *x) noexcept {
	if (x == nullptr) return;

	lsl::factory *factory = reinterpret_cast<sample *>(x)->factory_;

	// delete the underlying memory only if it wasn't allocated in the factory's storage area
	if (x < factory->storage_ || x >= factory->storage_ + factory->storage_size_)
		delete[] (char *)x;
}

/// ensure that a given value is a multiple of some base, round up if necessary
constexpr uint32_t ensure_multiple(uint32_t v, unsigned base) {
	return (v % base) ? v - (v % base) + base : v;
}

// Sample functions

lsl::sample::~sample() noexcept {
	if (format_ == cft_string)
		for (auto &val : samplevals<std::string>(*this)) val.~basic_string<char>();
}

bool sample::operator==(const sample &rhs) const noexcept {
	if ((timestamp_ != rhs.timestamp_) || (format_ != rhs.format_) ||
		(num_channels_ != rhs.num_channels_))
		return false;
	if (format_ != cft_string) return memcmp(&(rhs.data_), &data_, datasize()) == 0;

	// For string values, each value has to be compared individually
	auto thisvals = samplevals<const std::string>(*this);
	return std::equal(thisvals.begin(), thisvals.end(), samplevals<std::string>(rhs).begin());
}

template <class T> void lsl::sample::assign_typed(const T *src) {
	switch (format_) {
	case cft_float32: conv_from<float>(src); break;
	case cft_double64: conv_from<double>(src); break;
	case cft_int8: conv_from<int8_t>(src); break;
	case cft_int16: conv_from<int16_t>(src); break;
	case cft_int32: conv_from<int32_t>(src); break;
#ifndef BOOST_NO_INT64_T
	case cft_int64: conv_from<int64_t>(src); break;
#endif
	case cft_string: conv_from<std::string>(src); break;
	default: throw std::invalid_argument("Unsupported channel format.");
	}
}

template <class T> void lsl::sample::retrieve_typed(T *dst) {
	switch (format_) {
	case cft_float32: conv_into<float>(dst); break;
	case cft_double64: conv_into<double>(dst); break;
	case cft_int8: conv_into<int8_t>(dst); break;
	case cft_int16: conv_into<int16_t>(dst); break;
	case cft_int32: conv_into<int32_t>(dst); break;
#ifndef BOOST_NO_INT64_T
	case cft_int64: conv_into<int64_t>(dst); break;
#endif
	case cft_string: conv_into<std::string>(dst); break;
	default: throw std::invalid_argument("Unsupported channel format.");
	}
}

void lsl::sample::assign_untyped(const void *newdata) {
	if (format_ != cft_string)
		memcpy(&data_, newdata, datasize());
	else
		throw std::invalid_argument("Cannot assign untyped data to a string-formatted sample.");
}

void lsl::sample::retrieve_untyped(void *newdata) {
	if (format_ != cft_string)
		memcpy(newdata, &data_, datasize());
	else
		throw std::invalid_argument("Cannot retrieve untyped data from a string-formatted sample.");
}

/// Helper function to save raw binary data to a stream buffer.
void save_raw(std::streambuf &sb, const void *address, std::size_t count) {
	if ((std::size_t)sb.sputn((const char *)address, (std::streamsize)count) != count)
		throw std::runtime_error("Output stream error.");
}

void save_byte(std::streambuf &sb, uint8_t v) {
	if (sb.sputc(*reinterpret_cast<const char *>(&v)) == std::streambuf::traits_type::eof())
		throw std::runtime_error("Output stream error.");
}

/// Helper function to load raw binary data from a stream buffer.
void load_raw(std::streambuf &sb, void *address, std::size_t count) {
	if ((std::size_t)sb.sgetn((char *)address, (std::streamsize)count) != count)
		throw std::runtime_error("Input stream error.");
}

uint8_t load_byte(std::streambuf &sb) {
	auto res = sb.sbumpc();
	if (res == std::streambuf::traits_type::eof()) throw std::runtime_error("Input stream error.");
	return static_cast<uint8_t>(res);
}

/// Load a value from a stream buffer with correct endian treatment.
template <typename T> T load_value(std::streambuf &sb, bool reverse_byte_order) {
	T tmp;
	load_raw(sb, &tmp, sizeof(T));
	if (sizeof(T) > 1 && reverse_byte_order) endian_reverse_inplace(tmp);
	return tmp;
}

/// Save a value to a stream buffer with correct endian treatment.
template <typename T> void save_value(std::streambuf &sb, T v, bool reverse_byte_order) {
	if (sizeof(T) > 1 && reverse_byte_order) endian_reverse_inplace(v);
	save_raw(sb, &v, sizeof(T));
}

void sample::save_streambuf(
	std::streambuf &sb, int /*protocol_version*/, bool reverse_byte_order, void *scratchpad) const {
	// write sample header
	if (timestamp_ == DEDUCED_TIMESTAMP) {
		save_byte(sb, TAG_DEDUCED_TIMESTAMP);
	} else {
		save_byte(sb, TAG_TRANSMITTED_TIMESTAMP);
		save_value(sb, timestamp_, reverse_byte_order);
	}
	// write channel data
	if (format_ == cft_string) {
		for (const auto &str : samplevals<std::string>(*this)) {
			// write string length as variable-length integer
			if (str.size() <= 0xFF) {
				save_byte(sb, static_cast<uint8_t>(sizeof(uint8_t)));
				save_byte(sb, static_cast<uint8_t>(str.size()));
			} else {
				if (str.size() <= 0xFFFFFFFF) {
					save_byte(sb, static_cast<uint8_t>(sizeof(uint32_t)));
					save_value(sb, static_cast<uint32_t>(str.size()), reverse_byte_order);
				} else {
					save_byte(sb, static_cast<uint8_t>(sizeof(uint64_t)));
					save_value(sb, static_cast<std::size_t>(str.size()), reverse_byte_order);
				}
			}
			// write string contents
			if (!str.empty()) save_raw(sb, str.data(), str.size());
		}
	} else {
		// write numeric data in binary
		if (!reverse_byte_order || format_sizes[format_] == 1) {
			save_raw(sb, &data_, datasize());
		} else {
			memcpy(scratchpad, &data_, datasize());
			convert_endian(scratchpad, num_channels_, format_sizes[format_]);
			save_raw(sb, scratchpad, datasize());
		}
	}
}

void sample::load_streambuf(
	std::streambuf &sb, int /*unused*/, bool reverse_byte_order, bool suppress_subnormals) {
	// read sample header
	if (load_byte(sb) == TAG_DEDUCED_TIMESTAMP)
		// deduce the timestamp
		timestamp_ = DEDUCED_TIMESTAMP;
	else
		// read the time stamp
		timestamp_ = load_value<double>(sb, reverse_byte_order);

	// read channel data
	if (format_ == cft_string) {
		for (auto &str : samplevals<std::string>(*this)) {
			// read string length as variable-length integer
			std::size_t len = 0;
			auto lenbytes = load_byte(sb);

			if (sizeof(std::size_t) < 8 && lenbytes > sizeof(std::size_t))
				throw std::runtime_error(
					"This platform does not support strings of 64-bit length.");
			switch (lenbytes) {
			case sizeof(uint8_t): len = load_byte(sb); break;
			case sizeof(uint16_t): len = load_value<uint16_t>(sb, reverse_byte_order); break;
			case sizeof(uint32_t): len = load_value<uint32_t>(sb, reverse_byte_order); break;
#ifndef BOOST_NO_INT64_T
			case sizeof(uint64_t): len = load_value<uint64_t>(sb, reverse_byte_order); break;
#endif
			default: throw std::runtime_error("Stream contents corrupted (invalid varlen int).");
			}
			// read string contents
			str.resize(len);
			if (len > 0) load_raw(sb, (void*) str.data(), len);
		}
	} else {
		// read numeric channel data
		load_raw(sb, &data_, datasize());
		if (reverse_byte_order && format_sizes[format_] > 1)
			convert_endian(&data_, num_channels(), format_sizes[format_]);
		if (suppress_subnormals && format_float[format_]) {
			if (format_ == cft_float32) {
				for (auto &val : samplevals<uint32_t>(*this))
					if (val && ((val & UINT32_C(0x7fffffff)) <= UINT32_C(0x007fffff)))
						val &= UINT32_C(0x80000000);
			} else {
#ifndef BOOST_NO_INT64_T
				for (auto &val : samplevals<uint64_t>(*this))
					if (val &&
						((val & UINT64_C(0x7fffffffffffffff)) <= UINT64_C(0x000fffffffffffff)))
						val &= UINT64_C(0x8000000000000000);
#endif
			}
		}
	}
}

void lsl::sample::convert_endian(void *data, uint32_t n, uint32_t width) {
	void *dataptr = reinterpret_cast<void *>(data);
	switch (width) {
	case 1: break;
	case sizeof(int16_t):
		for (auto &val : dataiter<int16_t>(dataptr, n)) endian_reverse_inplace(val);
		break;
	case sizeof(int32_t):
		for (auto &val : dataiter<int32_t>(dataptr, n)) endian_reverse_inplace(val);
		break;
	case sizeof(int64_t):
		for (auto &val : dataiter<int64_t>(dataptr, n)) endian_reverse_inplace(val);
		break;
	default: throw std::runtime_error("Unsupported channel format for endian conversion.");
	}
}

template <class Archive> void sample::serialize_channels(Archive &ar, const uint32_t /*unused*/) {
	switch (format_) {
	case cft_float32:
		for (auto &val : samplevals<float>(*this)) ar &val;
		break;
	case cft_double64:
		for (auto &val : samplevals<double>(*this)) ar &val;
		break;
	case cft_string:
		for (auto &val : samplevals<std::string>(*this)) ar &val;
		break;
	case cft_int8:
		for (auto &val : samplevals<int8_t>(*this)) ar &val;
		break;
	case cft_int16:
		for (auto &val : samplevals<int16_t>(*this)) ar &val;
		break;
	case cft_int32:
		for (auto &val : samplevals<int32_t>(*this)) ar &val;
		break;
#ifndef BOOST_NO_INT64_T
	case cft_int64:
		for (auto &val : samplevals<int64_t>(*this)) ar &val;
		break;
#endif
	default: throw std::runtime_error("Unsupported channel format.");
	}
}

void lsl::sample::serialize(eos::portable_oarchive &ar, const uint32_t archive_version) const {
	// write sample header
	if (timestamp_ == DEDUCED_TIMESTAMP) {
		ar &TAG_DEDUCED_TIMESTAMP;
	} else {
		ar &TAG_TRANSMITTED_TIMESTAMP &timestamp_;
	}
	// write channel data
	const_cast<sample *>(this)->serialize_channels(ar, archive_version);
}

void lsl::sample::serialize(eos::portable_iarchive &ar, const uint32_t archive_version) {
	// read sample header
	char tag;
	ar &tag;
	if (tag == TAG_DEDUCED_TIMESTAMP) {
		// deduce the timestamp
		timestamp_ = DEDUCED_TIMESTAMP;
	} else {
		// read the time stamp
		ar &timestamp_;
	}
	// read channel data
	serialize_channels(ar, archive_version);
}

template <typename T> void test_pattern(T *data, uint32_t num_channels, int offset) {
	for (std::size_t k = 0; k < num_channels; k++) {
		std::size_t val = k + static_cast<std::size_t>(offset);
		if (std::is_integral_v<T>) val %= static_cast<std::size_t>(std::numeric_limits<T>::max());
		data[k] = (k % 2 == 0) ? static_cast<T>(val) : -static_cast<T>(val);
	}
}

sample &sample::assign_test_pattern(int offset) {
	pushthrough = true;
	timestamp_ = 123456.789;

	switch (format_) {
	case cft_float32:
		test_pattern(samplevals<float>(*this).begin(), num_channels_, offset + 0);
		break;
	case cft_double64:
		test_pattern(samplevals<double>(*this).begin(), num_channels_, offset + 16777217);
		break;
	case cft_string: {
		std::string *data = samplevals<std::string>(*this).begin();
		for (int32_t k = 0U; k < (int)num_channels_; k++)
			data[k] = to_string((k + 10) * (k % 2 == 0 ? 1 : -1));
		break;
	}
	case cft_int32:
		test_pattern(samplevals<int32_t>(*this).begin(), num_channels_, offset + 65537);
		break;
	case cft_int16:
		test_pattern(samplevals<int16_t>(*this).begin(), num_channels_, offset + 257);
		break;
	case cft_int8:
		test_pattern(samplevals<int8_t>(*this).begin(), num_channels_, offset + 1);
		break;
#ifndef BOOST_NO_INT64_T
	case cft_int64: {
		int64_t *data = samplevals<int64_t>(*this).begin();
		int64_t offset64 = 2147483649LL + offset;
		for (uint32_t k = 0; k < num_channels_; k++) {
			data[k] = (k + offset64);
			if (k % 2 == 1) data[k] = -data[k];
		}
		break;
	}
#endif
	default: throw std::invalid_argument("Unsupported channel format used to construct a sample.");
	}

	return *this;
}

lsl::sample::sample(lsl_channel_format_t fmt, uint32_t num_channels, factory *fact)
	: format_(fmt), num_channels_(num_channels), refcount_(0), next_(nullptr), factory_(fact) {
	// construct std::strings in the data section via placement new
	if (format_ == cft_string)
		for (auto &val : samplevals<std::string>(*this)) new (&val) std::string();
}

factory::factory(lsl_channel_format_t fmt, uint32_t num_chans, uint32_t num_reserve)
	: fmt_(fmt), num_chans_(num_chans),
	  sample_size_(ensure_multiple(
		  sizeof(sample) - sizeof(sample::data_) + format_sizes[fmt] * num_chans, 16)),
	  storage_size_(sample_size_ * std::max(2U, num_reserve + 1)),
	  storage_(new char[storage_size_]), head_(sentinel()), tail_(sentinel()) {
	// pre-construct an array of samples in the storage area and chain into a freelist
	// this is functionally identical to calling `reclaim_sample()` for each sample, but alters
	// the head_/tail_ positions only once
	sample *s = sample_by_index(0);
	for (char *p = storage_, *e = p + storage_size_; p < e;) {
		s = new (reinterpret_cast<sample *>(p)) sample(fmt, num_chans, this);
		s->next_ = reinterpret_cast<sample *>(p += sample_size_);
	}
	s->next_ = nullptr;
	head_.store(s);
}

sample_p factory::new_sample(double timestamp, bool pushthrough) {
	sample *result;
	// try to retrieve a free sample, adding fresh samples until it succeeds
	while ((result = pop_freelist()) == nullptr)
		reclaim_sample(new (new char[sample_size_]) sample(fmt_, num_chans_, this));

	result->timestamp_ = timestamp;
	result->pushthrough = pushthrough;
	return {result};
}

sample *factory::pop_freelist() {
	sample *tail = tail_, *next = tail->next_.load(std::memory_order_acquire);
	if (tail == sentinel()) {
		// no samples available
		if (!next) return nullptr;
		tail_.store(next, std::memory_order_relaxed);
		tail = next;
		next = next->next_.load(std::memory_order_acquire);
	}
	if (next) {
		tail_.store(next, std::memory_order_relaxed);
		return tail;
	}
	sample *head = head_.load(std::memory_order_acquire);
	//
	if (tail != head) return nullptr;
	reclaim_sample(sentinel());
	next = tail->next_.load(std::memory_order_acquire);
	if (next) {
		tail_ = next;
		return tail;
	}
	return nullptr;
}

factory::~factory() {
	for (sample *cur = tail_, *next = cur->next_;; cur = next, next = next->next_) {
		if (cur != sentinel()) delete cur;
		if (!next) break;
	}
	delete[] storage_;
}

void factory::reclaim_sample(sample *s) {
	s->next_.store(nullptr, std::memory_order_release); // TODO: might be _relaxed?
	sample *prev = head_.exchange(s, std::memory_order_acq_rel);
	prev->next_.store(s, std::memory_order_release);
}

// template instantiations
template void lsl::sample::assign_typed(float const *);
template void lsl::sample::assign_typed(double const *);
template void lsl::sample::assign_typed(char const *);
template void lsl::sample::assign_typed(int16_t const *);
template void lsl::sample::assign_typed(int32_t const *);
template void lsl::sample::assign_typed(int64_t const *);
template void lsl::sample::assign_typed(std::string const *);
template void lsl::sample::retrieve_typed(float *);
template void lsl::sample::retrieve_typed(double *);
template void lsl::sample::retrieve_typed(char *);
template void lsl::sample::retrieve_typed(int16_t *);
template void lsl::sample::retrieve_typed(int32_t *);
template void lsl::sample::retrieve_typed(int64_t *);
template void lsl::sample::retrieve_typed(std::string *);
