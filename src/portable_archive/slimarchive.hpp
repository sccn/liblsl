#pragma once

/// Small shim implementing the needed Boost serialization classes for liblsl

#include <cstdint>
#include <stdexcept>
#include <streambuf>
#include <type_traits>

/// dummy #defines to prevent portable archive from loading additional boost archive parts
#define NO_EXPLICIT_TEMPLATE_INSTANTIATION
#define BOOST_ARCHIVE_SERIALIZER_INCLUDED
#define BOOST_SERIALIZATION_REGISTER_ARCHIVE(typename)

// forward declaration
namespace lsl {
class sample;
}

/// Maps LSL types to an index without needing full RTTI
template <typename T> struct lsl_type_index {};
template <> struct lsl_type_index<lsl::sample> { static const int idx = 0; };
/// two classes used in unit tests
template <> struct lsl_type_index<struct Testclass> { static const int idx = 1; };
template <> struct lsl_type_index<struct Testclass2> { static const int idx = 2; };

/// keep track of classes already seen once, needed for a field in Boost.Archive
class serialized_class_tracker {
	bool seen_[3]{false};

public:
	/// Return true /if an instance of this class has already been (de)serialized before
	template <typename T> inline bool already_seen(const T &) {
		if (seen_[lsl_type_index<T>::idx]) return true;
		seen_[lsl_type_index<T>::idx] = true;
		return false;
	}
};

namespace lslboost {
#ifndef BOOST_INTEGER_HPP
/// Maps a type size in bits to the corresponding uintXY_t type
template <int Size> struct uint_t {};
template <> struct uint_t<8> { using least = uint8_t; };
template <> struct uint_t<16> { using least = uint16_t; };
template <> struct uint_t<32> { using least = uint32_t; };
template <> struct uint_t<64> { using least = uint64_t; };
#endif

namespace archive {
using library_version_type = uint8_t;
inline library_version_type BOOST_ARCHIVE_VERSION() { return 17; }
enum flags { no_header = 1, no_codecvt = 2 };

struct archive_exception : std::exception {
	archive_exception(int) {}
	enum errors { invalid_signature, unsupported_version, other_exception };
};

/// Common class to store flags
class flagstore {
	int flags_;

public:
	flagstore(int flags) : flags_(flags) {}
	int get_flags() const { return flags_; }
};

template <typename Archive> struct basic_binary_oarchive : public flagstore {
	basic_binary_oarchive(int flags_) : flagstore(flags_) {}
};

template <typename Archive> struct basic_binary_iarchive : public flagstore {
	basic_binary_iarchive(int flags_) : flagstore(flags_) {}
};

/// Helper base class for calling methods of an descendant class, i.e. Archive::fn, not this->fn
template <typename Archive> class archive_upcaster {
public:
	archive_upcaster() {
		static_assert(
			std::is_base_of<typename std::remove_reference<decltype(*this)>::type, Archive>::value,
			"Archive must inherit from basic_binary_i/oprimitive");
	}

	/** Return a reference to the actual archive implementation instead of the base class
	 *
	 * This is needed for most operations to call Archive::fn, not basic_binary_primitive::fn */
	inline Archive &archive_impl() { return *((Archive *)this); }
};

/*
template<typename Archive> class basic_binary_primitive {
	std::streambuf &sb;
}*/

template <typename Archive, typename CharT, typename OSTraits>
class basic_binary_oprimitive : private serialized_class_tracker,
								private archive_upcaster<Archive> {
	std::streambuf &sb;

	/// calls Archive::serialize instead of this->serialize()
	template <typename T> Archive &delegate_save(const T &t) {
		this->archive_impl().save(t);
		return this->archive_impl();
	}

public:
	basic_binary_oprimitive(std::streambuf &sbuf, int) : archive_upcaster<Archive>(), sb(sbuf) {}

	template <typename T> void save_binary(const T *data, std::size_t bytes) {
		sb.sputn(reinterpret_cast<const char *>(data), bytes);
	}

	template <typename T> void save(const T &t) { save_binary<T>(&t, sizeof(T)); }

	void save(const std::string &s) {
		delegate_save(s.size());
		save_binary(s.data(), s.size());
	}

	template <typename T>
	std::enable_if_t<std::is_arithmetic<T>::value, Archive &> operator<<(const T &t) {
		return delegate_save(t);
	}

	Archive &operator<<(const std::string &t) { return delegate_save(t); }

	/// calls the `serialize()` member function if it exists
	template <typename T, void (T::*fn)(Archive &, uint32_t) const = &T::serialize>
	Archive &operator<<(const T &t) {
		if (!this->already_seen(t)) save<uint16_t>(0);
		t.serialize(this->archive_impl(), 0);
		return this->archive_impl();
	}

	template <typename T> Archive &operator&(const T &t) { return this->archive_impl() << t; }
};


template <class Archive, typename CharT, typename OSTraits>
class basic_binary_iprimitive : private serialized_class_tracker,
								private archive_upcaster<Archive> {
	std::streambuf &sb;

	template <typename T> Archive &delegate_load(T &t) {
		this->archive_impl().load(t);
		return this->archive_impl();
	}

public:
	basic_binary_iprimitive(std::streambuf &sbuf, int) : archive_upcaster<Archive>(), sb(sbuf) {}

	/// Load raw binary data from the stream
	template <typename T> void load_binary(T *data, std::size_t bytes) {
		sb.sgetn(reinterpret_cast<char *>(data), bytes);
	}

	template <typename T> void load(T &t) { load_binary<T>(&t, sizeof(T)); }

	void load(std::string &s) {
		std::size_t size;
		delegate_load(size);
		char *data = new char[size];
		load_binary(data, size);
		s.assign(data, size);
		delete[] data;
	}

	Archive &operator>>(std::string &t) { return delegate_load(t); }

	template <typename T>
	std::enable_if_t<std::is_arithmetic<T>::value, Archive &> operator>>(T &t) {
		return delegate_load(t);
	}

	/// calls the `serialize()` member function if it exists
	template <typename T, void (T::*fn)(Archive &, uint32_t) = &T::serialize>
	Archive &operator>>(T &t) {
		if (!this->already_seen(t)) {
			uint16_t dummy;
			load(dummy);
		}
		t.serialize(this->archive_impl(), 0);
		return this->archive_impl();
	}

	template <typename T> Archive &operator&(T &t) { return this->archive_impl() >> t; }

	void set_library_version(int) {}
};
} // namespace archive
} // namespace lslboost
