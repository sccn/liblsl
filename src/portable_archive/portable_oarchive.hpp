#pragma once

#include "portable_archive_includes.hpp"
#include "slimarchive.hpp"
#include <cstring>
#include <ostream>

namespace eos {

// IMPORTANT: We are fixing the lslboost serialization archive version
//		      at 9; if you upgrade your lslboost distribution
//			  you may at some point pull in a breaking change which
//            will break LSL protocol version 1.00 compatibility.
//            This does not affect LSL protocols 1.10 or later.
const archive_version_type FIXED_VERSION = archive_version_type(9);

// forward declaration
class portable_oarchive;

using portable_oprimitive = lslboost::archive::basic_binary_oprimitive<portable_oarchive,
	std::ostream::char_type, std::ostream::traits_type>;

/**
 * \brief Portable binary output archive using little endian format.
 *
 * This archive addresses integer size, endianness and floating point types so
 * that data can be transferred across different systems. The archive consists
 * primarily of three different save implementations for integral types,
 * floating point types and string types. Those functions are templates and use
 * enable_if to be correctly selected for overloading.
 *
 * \note The class is based on the portable binary example by Robert Ramey and
 *       uses Beman Dawes endian library plus fp_utilities by Johan Rade.
 */
class portable_oarchive : public portable_oprimitive

	// the example derives from common_oarchive but that lacks the
	// save_override functions so we chose to stay one level higher
	,
						  public lslboost::archive::basic_binary_oarchive<portable_oarchive> {
	// workaround for gcc: use a dummy struct
	// as additional argument type for overloading
	template <int> struct dummy {
		dummy(int) {}
	};

	// stores a signed char directly to stream
	inline void save_signed_char(const signed char &c) { portable_oprimitive::save(c); }

	// archive initialization
	void init(unsigned flags) {
		// it is vital to have version information if the archive is
		// to be parsed with a newer version of lslboost::serialization
		// therefor we create a header, no header means lslboost 1.33
		if (flags & lslboost::archive::no_header)
			assert(archive_version == 3);
		else {
			// write our minimalistic header (magic byte plus version)
			// the lslboost archives write a string instead - by calling
			// lslboost::archive::basic_binary_oarchive<derived_t>::init()
			save_signed_char(magic_byte);

			// write current version
			//				save<unsigned>(archive_version);
			operator<<(FIXED_VERSION);
		}
	}

public:
	/**
	 * \brief Constructor on a stream using ios::binary mode!
	 *
	 * We cannot call basic_binary_oprimitive::init which stores type
	 * sizes to the archive in order to detect transfers to non-compatible
	 * platforms.
	 *
	 * We could have called basic_binary_oarchive::init which would create
	 * the lslboost::serialization standard archive header containing also the
	 * library version. Due to efficiency we stick with our own.
	 */
	portable_oarchive(std::ostream &os, unsigned flags = 0)
		: portable_oprimitive(*os.rdbuf(), flags & lslboost::archive::no_codecvt),
		  lslboost::archive::basic_binary_oarchive<portable_oarchive>(flags) {
		init(flags);
	}

	portable_oarchive(std::streambuf &sb, unsigned flags = 0)
		: portable_oprimitive(sb, flags & lslboost::archive::no_codecvt),
		  lslboost::archive::basic_binary_oarchive<portable_oarchive>(flags) {
		init(flags);
	}

	//! Save narrow strings.
	void save(const std::string &s) { portable_oprimitive::save(s); }

	/**
	 * \brief Saving bool type.
	 *
	 * Saving bool directly, not by const reference
	 * because of tracking_type's operator (bool).
	 *
	 * \note If you cannot compile your application and it says something
	 * about save(bool) cannot convert your type const A& into bool then
	 * you should check your BOOST_CLASS_IMPLEMENTATION setting for A, as
	 * portable_archive is not able to handle custom primitive types in
	 * a general manner.
	 */
	void save(const bool &b) {
		save_signed_char(b);
		if (b) save_signed_char('T');
	}

	/**
	 * \brief Save integer types.
	 *
	 * First we save the size information ie. the number of bytes that hold the
	 * actual data. We subsequently transform the data using store_little_endian
	 * and store non-zero bytes to the stream.
	 */
	template <typename T>
	typename std::enable_if<std::is_integral<T>::value>::type save(const T &t, dummy<2> = 0) {
		if (T temp = t) {
			// examine the number of bytes
			// needed to represent the number
			signed char size = 0;
			if (sizeof(T) == 1)
				size = 1;
			else {
				do {
					temp >>= CHAR_BIT;
					++size;
				} while (temp != 0 && temp != (T)-1);
			}

			// encode the sign bit into the size
			save_signed_char(t > 0 ? size : -size);
			BOOST_ASSERT(t > 0 || std::is_signed<T>::value);

			// we choose to use little endian because this way we just
			// save the first size bytes to the stream and skip the rest
			temp = endian::native_to_little(t);
			save_binary(&temp, size);
		}
		// zero optimization
		else
			save_signed_char(0);
	}

	/**
	 * \brief Save floating point types.
	 *
	 * We simply rely on fp_traits to extract the bit pattern into an (unsigned)
	 * integral type and store that into the stream. Francois Mauger provided
	 * standardized behaviour for special values like inf and NaN, that need to
	 * be serialized in his application.
	 *
	 * \note by Johan Rade (author of the floating point utilities library):
	 * Be warned that the math::detail::fp_traits<T>::type::get_bits() function
	 * is *not* guaranteed to give you all bits of the floating point number. It
	 * will give you all bits if and only if there is an integer type that has
	 * the same size as the floating point you are copying from. It will not
	 * give you all bits for double if there is no uint64_t. It will not give
	 * you all bits for long double if sizeof(long double) > 8 or there is no
	 * uint64_t.
	 *
	 * The member fp_traits<T>::type::coverage will tell you whether all bits
	 * are copied. This is a typedef for either math::detail::all_bits or
	 * math::detail::not_all_bits.
	 *
	 * If the function does not copy all bits, then it will copy the most
	 * significant bits. So if you serialize and deserialize the way you
	 * describe, and fp_traits<T>::type::coverage is math::detail::not_all_bits,
	 * then your floating point numbers will be truncated. This will introduce
	 * small rounding off errors.
	 */
	template <typename T>
	typename std::enable_if<std::is_floating_point<T>::value>::type save(const T &t, dummy<3> = 0) {
		using traits = typename fp::detail::fp_traits<T>::type;
		using newtraits = typename lsl::detail::fp_traits<T>::type;
		static_assert(std::is_same<typename traits::bits, typename newtraits::bits>::value,
			"Wrong corresponding int type");

		// if the no_infnan flag is set we must throw here
		if (get_flags() & no_infnan && !fp::isfinite(t)) throw portable_archive_exception(t);

		// if you end here there are three possibilities:
		// 1. you're serializing a long double which is not portable
		// 2. you're serializing a double but have no 64 bit integer
		// 3. your machine is using an unknown floating point format
		// after reading the note above you still might decide to
		// deactivate this static assert and try if it works out.
		typename traits::bits bits;
		static_assert(sizeof(bits) == sizeof(T), "mismatching type sizes");
		static_assert(
			std::numeric_limits<T>::is_iec559, "float representation differs from IEC559");

		static_assert(traits::exponent == newtraits::exponent, "mismatching exponent");
		static_assert(traits::significand == newtraits::significand, "mismatching significand");
		static_assert(traits::sign == newtraits::sign, "mismatching sign bit");

		switch (fp::fpclassify(t)) {
		case FP_NAN: bits = traits::exponent | traits::significand; break;
		case FP_INFINITE: bits = traits::exponent | (t < 0) * traits::sign; break;
		case FP_SUBNORMAL: assert(std::numeric_limits<T>::has_denorm); // pass
		case FP_ZERO: // note that floats can be ±0.0
		case FP_NORMAL: traits::get_bits(t, bits); break;
		default: throw portable_archive_exception(t);
		}

		save(bits);
	}

	// in lslboost 1.44 version_type was splitted into library_version_type and
	// item_version_type, plus a whole bunch of additional strong typedefs.
	template <typename T>
	typename std::enable_if<!std::is_arithmetic<T>::value>::type save(const T &t, dummy<4> = 0) {
		// we provide a generic save routine for all types that feature
		// conversion operators into an unsigned integer value like those
		// created through BOOST_STRONG_TYPEDEF(X, some unsigned int) like
		// library_version_type, collection_size_type, item_version_type,
		// class_id_type, object_id_type, version_type and tracking_type
		save((typename lslboost::uint_t<sizeof(T) * CHAR_BIT>::least)(t));
	}
};
} // namespace eos
