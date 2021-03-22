#define BOOST_MATH_DISABLE_STD_FPCLASSIFY
#include "sample.h"
#include "portable_archive/portable_iarchive.hpp"
#include "portable_archive/portable_oarchive.hpp"

using namespace lsl;

sample::~sample() noexcept{
	if (format_ != cft_string) return;
	for (std::string *p = (std::string *)&data_, *e = p + num_channels_; p < e; ++p)
		p->~basic_string<char>();
}

void sample::operator delete(void *x) noexcept {
	if(x == nullptr) return;

	lsl::factory *factory = reinterpret_cast<sample *>(x)->factory_;

	// delete the underlying memory only if it wasn't allocated in the factory's storage area
	if (x < factory->storage_ || x >= factory->storage_ + factory->storage_size_)
		delete[](char *) x;
}

bool sample::operator==(const sample &rhs) const noexcept {
	if ((timestamp != rhs.timestamp) || (format_ != rhs.format_) ||
		(num_channels_ != rhs.num_channels_))
		return false;
	if (format_ != cft_string)
		return memcmp(&(rhs.data_), &data_, datasize()) == 0;
	else {
		std::string *data = (std::string *)&data_;
		std::string *rhsdata = (std::string *)&(rhs.data_);
		for (std::size_t k = 0; k < num_channels_; k++)
			if (data[k] != rhsdata[k]) return false;
		return true;
	}
}

sample &sample::assign_typed(const std::string *s) {
	switch (format_) {
	case cft_string:
		for (std::string *p = (std::string *)&data_, *e = p + num_channels_; p < e; *p++ = *s++)
			;
		break;
	case cft_float32:
		for (float *p = (float *)&data_, *e = p + num_channels_; p < e;
			 *p++ = from_string<float>(*s++))
			;
		break;
	case cft_double64:
		for (double *p = (double *)&data_, *e = p + num_channels_; p < e;
			 *p++ = from_string<double>(*s++))
			;
		break;
	case cft_int8:
		for (int8_t *p = (int8_t *)&data_, *e = p + num_channels_; p < e;
			 *p++ = from_string<int8_t>(*s++))
			;
		break;
	case cft_int16:
		for (int16_t *p = (int16_t *)&data_, *e = p + num_channels_; p < e;
			 *p++ = from_string<int16_t>(*s++))
			;
		break;
	case cft_int32:
		for (int32_t *p = (int32_t *)&data_, *e = p + num_channels_; p < e;
			 *p++ = from_string<int32_t>(*s++))
			;
		break;
#ifndef BOOST_NO_INT64_T
	case cft_int64:
		for (int64_t *p = (int64_t *)&data_, *e = p + num_channels_; p < e;
			 *p++ = from_string<int64_t>(*s++))
			;
		break;
#endif
	default: throw std::invalid_argument("Unsupported channel format.");
	}
	return *this;
}

sample &sample::retrieve_typed(std::string *d) {
	switch (format_) {
	case cft_string:
		for (std::string *p = (std::string *)&data_, *e = p + num_channels_; p < e; *d++ = *p++)
			;
		break;
	case cft_float32:
		for (float *p = (float *)&data_, *e = p + num_channels_; p < e; *d++ = to_string(*p++))
			;
		break;
	case cft_double64:
		for (double *p = (double *)&data_, *e = p + num_channels_; p < e; *d++ = to_string(*p++))
			;
		break;
	case cft_int8:
		for (int8_t *p = (int8_t *)&data_, *e = p + num_channels_; p < e; *d++ = to_string(*p++))
			;
		break;
	case cft_int16:
		for (int16_t *p = (int16_t *)&data_, *e = p + num_channels_; p < e; *d++ = to_string(*p++))
			;
		break;
	case cft_int32:
		for (int32_t *p = (int32_t *)&data_, *e = p + num_channels_; p < e; *d++ = to_string(*p++))
			;
		break;
#ifndef BOOST_NO_INT64_T
	case cft_int64:
		for (int64_t *p = (int64_t *)&data_, *e = p + num_channels_; p < e; *d++ = to_string(*p++))
			;
		break;
#endif
	default: throw std::invalid_argument("Unsupported channel format.");
	}
	return *this;
}

/// Helper function to save raw binary data to a stream buffer.
void save_raw(std::streambuf &sb, const void *address, std::size_t count) {
	if ((std::size_t)sb.sputn((const char *)address, (std::streamsize)count) != count)
		throw std::runtime_error("Output stream error.");
}

/// Helper function to load raw binary data from a stream buffer.
void load_raw(std::streambuf &sb, void *address, std::size_t count) {
	if ((std::size_t)sb.sgetn((char *)address, (std::streamsize)count) != count)
		throw std::runtime_error("Input stream error.");
}

/// Load a value from a stream buffer with correct endian treatment.
template <typename T> T load_value(std::streambuf &sb, int use_byte_order) {
	T tmp;
	load_raw(sb, &tmp, sizeof(T));
	if (sizeof(T) > 1 && use_byte_order != BOOST_BYTE_ORDER) endian_reverse_inplace(tmp);
	return tmp;
}

/// Save a value to a stream buffer with correct endian treatment.
template <typename T> void save_value(std::streambuf &sb, T v, int use_byte_order) {
	if (use_byte_order != BOOST_BYTE_ORDER) endian_reverse_inplace(v);
	save_raw(sb, &v, sizeof(T));
}

void sample::save_streambuf(
	std::streambuf &sb, int /*protocol_version*/, int use_byte_order, void *scratchpad) const {
	// write sample header
	if (timestamp == DEDUCED_TIMESTAMP) {
		save_value(sb, TAG_DEDUCED_TIMESTAMP, use_byte_order);
	} else {
		save_value(sb, TAG_TRANSMITTED_TIMESTAMP, use_byte_order);
		save_value(sb, timestamp, use_byte_order);
	}
	// write channel data
	if (format_ == cft_string) {
		for (std::string *p = (std::string *)&data_, *e = p + num_channels_; p < e; p++) {
			// write string length as variable-length integer
			if (p->size() <= 0xFF) {
				save_value(sb, (uint8_t)sizeof(uint8_t), use_byte_order);
				save_value(sb, (uint8_t)p->size(), use_byte_order);
			} else {
				if (p->size() <= 0xFFFFFFFF) {
					save_value(sb, (uint8_t)sizeof(uint32_t), use_byte_order);
					save_value(sb, (uint32_t)p->size(), use_byte_order);
				} else {
#ifndef BOOST_NO_INT64_T
					save_value(sb, (uint8_t)sizeof(uint64_t), use_byte_order);
					save_value(sb, (uint64_t)p->size(), use_byte_order);
#else
					save_value(sb, (uint8_t)sizeof(std::size_t), use_byte_order);
					save_value(sb, (std::size_t)p->size(), use_byte_order);
#endif
				}
			}
			// write string contents
			if (!p->empty()) save_raw(sb, p->data(), p->size());
		}
	} else {
		// write numeric data in binary
		if (use_byte_order == BOOST_BYTE_ORDER || format_sizes[format_] == 1) {
			save_raw(sb, &data_, datasize());
		} else {
			memcpy(scratchpad, &data_, datasize());
			convert_endian(scratchpad);
			save_raw(sb, scratchpad, datasize());
		}
	}
}

void sample::load_streambuf(std::streambuf &sb, int, int use_byte_order, bool suppress_subnormals) {
	// read sample header
	if (load_value<uint8_t>(sb, use_byte_order) == TAG_DEDUCED_TIMESTAMP)
		// deduce the timestamp
		timestamp = DEDUCED_TIMESTAMP;
	else
		// read the time stamp
		timestamp = load_value<double>(sb, use_byte_order);

	// read channel data
	if (format_ == cft_string) {
		for (std::string *p = (std::string *)&data_, *e = p + num_channels_; p < e; p++) {
			// read string length as variable-length integer
			std::size_t len = 0;
			auto lenbytes = load_value<uint8_t>(sb, use_byte_order);

			if (sizeof(std::size_t) < 8 && lenbytes > sizeof(std::size_t))
				throw std::runtime_error(
					"This platform does not support strings of 64-bit length.");
			switch (lenbytes) {
			case sizeof(uint8_t): len = load_value<uint8_t>(sb, use_byte_order); break;
			case sizeof(uint16_t): len = load_value<uint16_t>(sb, use_byte_order); break;
			case sizeof(uint32_t): len = load_value<uint32_t>(sb, use_byte_order); break;
#ifndef BOOST_NO_INT64_T
			case sizeof(uint64_t): len = load_value<uint64_t>(sb, use_byte_order); break;
#endif
			default: throw std::runtime_error("Stream contents corrupted (invalid varlen int).");
			}
			// read string contents
			p->resize(len);
			if (len > 0) load_raw(sb, &(*p)[0], len);
		}
	} else {
		// read numeric channel data
		load_raw(sb, &data_, datasize());
		if (use_byte_order != BOOST_BYTE_ORDER && format_sizes[format_] > 1) convert_endian(&data_);
		if (suppress_subnormals && format_float[format_]) {
			if (format_ == cft_float32) {
				for (uint32_t *p = (uint32_t *)&data_, *e = p + num_channels_; p < e; p++)
					if (*p && ((*p & UINT32_C(0x7fffffff)) <= UINT32_C(0x007fffff)))
						*p &= UINT32_C(0x80000000);
			} else {
#ifndef BOOST_NO_INT64_T
				for (uint64_t *p = (uint64_t *)&data_, *e = p + num_channels_; p < e; p++)
					if (*p && ((*p & UINT64_C(0x7fffffffffffffff)) <= UINT64_C(0x000fffffffffffff)))
						*p &= UINT64_C(0x8000000000000000);
#endif
			}
		}
	}
}

template <class Archive> void sample::serialize_channels(Archive &ar, const uint32_t) {
	switch (format_) {
	case cft_float32:
		for (float *p = (float *)&data_, *e = p + num_channels_; p < e; ar & *p++)
			;
		break;
	case cft_double64:
		for (double *p = (double *)&data_, *e = p + num_channels_; p < e; ar & *p++)
			;
		break;
	case cft_string:
		for (std::string *p = (std::string *)&data_, *e = p + num_channels_; p < e; ar & *p++)
			;
		break;
	case cft_int8:
		for (int8_t *p = (int8_t *)&data_, *e = p + num_channels_; p < e; ar & *p++)
			;
		break;
	case cft_int16:
		for (int16_t *p = (int16_t *)&data_, *e = p + num_channels_; p < e; ar & *p++)
			;
		break;
	case cft_int32:
		for (int32_t *p = (int32_t *)&data_, *e = p + num_channels_; p < e; ar & *p++)
			;
		break;
#ifndef BOOST_NO_INT64_T
	case cft_int64:
		for (int64_t *p = (int64_t *)&data_, *e = p + num_channels_; p < e; ar & *p++)
			;
		break;
#endif
	default: throw std::runtime_error("Unsupported channel format.");
	}
}

void sample::save(eos::portable_oarchive &ar, const uint32_t archive_version) const {
	// write sample header
	if (timestamp == DEDUCED_TIMESTAMP) {
		ar &TAG_DEDUCED_TIMESTAMP;
	} else {
		ar &TAG_TRANSMITTED_TIMESTAMP &timestamp;
	}
	// write channel data
	const_cast<sample *>(this)->serialize_channels(ar, archive_version);
}

void sample::load(eos::portable_iarchive &ar, const uint32_t archive_version) {
	// read sample header
	char tag;
	ar &tag;
	if (tag == TAG_DEDUCED_TIMESTAMP) {
		// deduce the timestamp
		timestamp = DEDUCED_TIMESTAMP;
	} else {
		// read the time stamp
		ar &timestamp;
	}
	// read channel data
	serialize_channels(ar, archive_version);
}

template <typename T> void test_pattern(T *data, uint32_t num_channels, int offset) {
	for (std::size_t k = 0; k < num_channels; k++) {
		std::size_t val = k + static_cast<std::size_t>(offset);
		if(std::is_integral<T>::value)
			val %= static_cast<std::size_t>(std::numeric_limits<T>::max());
		data[k] = ( k % 2 == 0) ? static_cast<T>(val) : -static_cast<T>(val);
	}
}

sample &sample::assign_test_pattern(int offset) {
	pushthrough = 1;
	timestamp = 123456.789;

	switch (format_) {
	case cft_float32:
		test_pattern(reinterpret_cast<float *>(&data_), num_channels_, offset + 0);
		break;
	case cft_double64:
		test_pattern(reinterpret_cast<double *>(&data_), num_channels_, offset + 16777217);
		break;
	case cft_string: {
		std::string *data = (std::string *)&data_;
		for (int32_t k = 0u; k < (int) num_channels_; k++)
			data[k] = to_string((k + 10) * (k % 2 == 0 ? 1 : -1));
		break;
	}
	case cft_int32:
		test_pattern(reinterpret_cast<int32_t *>(&data_), num_channels_, offset + 65537);
		break;
	case cft_int16:
		test_pattern(reinterpret_cast<int16_t *>(&data_), num_channels_, offset + 257);
		break;
	case cft_int8:
		test_pattern(reinterpret_cast<int8_t *>(&data_), num_channels_, offset + 1);
		break;
#ifndef BOOST_NO_INT64_T
	case cft_int64: {
		int64_t *data = (int64_t *)&data_;
		int64_t offset64 = 2147483649ll + offset;
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

factory::factory(lsl_channel_format_t fmt, uint32_t num_chans, uint32_t num_reserve)
	: fmt_(fmt), num_chans_(num_chans),
	  sample_size_(
		  ensure_multiple(sizeof(sample) - sizeof(char) + format_sizes[fmt] * num_chans, 16)),
	  storage_size_(sample_size_ * std::max(1u, num_reserve)),
	  storage_(new char[storage_size_ + sample_size_]), // +1 sample for the sentinel
	  sentinel_(new(reinterpret_cast<sample*>(storage_ + storage_size_)) sample(fmt, num_chans, this)),
	  head_(sentinel_), tail_(sentinel_)
	  {
	// pre-construct an array of samples in the storage area and chain into a freelist
	sample *s = nullptr;
	for (char *p = storage_, *e = p + storage_size_; p < e;) {
#pragma warning(suppress : 4291)
		s = new (reinterpret_cast<sample *>(p)) sample(fmt, num_chans, this);
		s->next_ = (sample *)(p += sample_size_);
	}
	s->next_ = nullptr;
	head_.store(s);
	sentinel_->next_ = (sample *)storage_;
}

sample_p factory::new_sample(double timestamp, bool pushthrough) {
	sample *result = pop_freelist();
	if (!result)
#pragma warning(suppress : 4291)
		result = new (new char[sample_size_]) sample(fmt_, num_chans_, this);
	result->timestamp = timestamp;
	result->pushthrough = pushthrough;
	return sample_p(result);
}

sample *factory::pop_freelist() {
	sample *tail = tail_, *next = tail->next_;
	if (tail == sentinel_) {
		if (!next) return nullptr;
		tail_ = next;
		tail = next;
		next = next->next_;
	}
	if (next) {
		tail_ = next;
		return tail;
	}
	sample *head = head_.load();
	if (tail != head) return nullptr;
	reclaim_sample(sentinel_);
	next = tail->next_;
	if (next) {
		tail_ = next;
		return tail;
	}
	return nullptr;
}

factory::~factory() {
	if (sample *cur = head_)
		for (sample *next = cur->next_; next; cur = next, next = next->next_) delete cur;
	delete[] storage_;
}

void factory::reclaim_sample(sample *s) {
	s->next_ = nullptr;
	sample *prev = head_.exchange(s);
	prev->next_ = s;
}
