/*****************************************************************************/
/**
 * \brief Provides an archive to read from and create portable binary files.
 * \author christian.pfligersdorffer@gmx.at
 * \version 5.1+
 *
 * This pair of archives brings the advantages of binary streams to the cross
 * platform lslboost::serialization user. While being almost as fast as the native
 * binary archive it allows its files to be exchanged between cpu architectures
 * using different byte order (endianness). Speaking of speed: in serializing
 * numbers the (portable) binary approach is approximately ten times faster than
 * the ascii implementation (that is inherently portable)!
 *
 * Based on the portable archive example by Robert Ramey this implementation
 * uses Beman Dawes endian library and fp_utilities from Johan Rade, both being
 * in boost since 1.36. Prior to that you need to add them both (header only)
 * to your boost directory before you're able to use the archives provided. 
 * Our archives have been tested successfully for boost versions 1.33 to 1.49!
 *
 * \note This version adds support for the USE_SHRINKWRAPPED_BOOST define, 
 *       which allows to toggle between a vanilla boost distribution and one with 
 *       mangled names (custom to LSL).
 *
 * \note Correct behaviour has so far been confirmed using PowerPC-32, x86-32
 *       and x86-64 platforms featuring different byte order. So there is a good
 *       chance it will instantly work for your specific setup. If you encounter
 *       problems or have suggestions please contact the author.
 *
 * \note Version 5.1 is now compatible with boost up to version 1.59. Thanks to
 *       ecotax for pointing to the issue with shared_ptr_helper.
 *
 * \note Version 5.0 is now compatible with boost up to version 1.49 and enables
 *       serialization of std::wstring by converting it to/from utf8 (thanks to
 *       Arash Abghari for this suggestion). With that all unit tests from the
 *       serialization library pass again with the notable exception of user
 *       defined primitive types. Those are not supported and as a result any
 *       user defined type to be used with the portable archives are required 
 *       to be at least object_serializable.
 *
 * \note Oliver Putz pointed out that -0.0 was not serialized correctly, so
 *       version 4.3 provides a fix for that. Thanks Ollie!
 *
 * \note Version 4.2 maintains compatibility with the latest boost 1.45 and adds
 *       serialization of special floating point values inf and NaN as proposed
 *       by Francois Mauger.
 *
 * \note Version 4.1 makes the archives work together with boost 1.40 and 1.41.
 *       Thanks to Francois Mauger for his suggestions.
 *
 * \note Version 4 removes one level of the inheritance hierarchy and directly
 *       builds upon binary primitive and basic binary archive, thereby fixing
 *       the last open issue regarding array serialization. Thanks to Robert
 *       Ramey for the hint.
 *
 * \note A few fixes introduced in version 3.1 let the archives pass all of the
 *       serialization tests. Thanks to Sergey Morozov for running the tests.
 *       Wouter Bijlsma pointed out where to find the fp_utilities and endian
 *       libraries headers inside the boost distribution. I would never have
 *       found them so thank him it works out of the box since boost 1.36.
 *
 * \note With Version 3.0 the archives have been made portable across different
 *       boost versions. For that purpose a header is added to the data that
 *       supplies the underlying serialization library version. Backwards
 *       compatibility is maintained by assuming library version boost 1.33 if
 *       the iarchive is created using the no_header flag. Whether a header is
 *       present or not can be guessed by peeking into the stream: the header's
 *       first byte is the magic number 127 coinciding with 'e'|'o'|'s' :-)
 *
 * \note Version 2.1 removes several compiler warnings and enhances floating
 *       point diagnostics to inform the user if some preconditions are violated
 *		 on his platform. We do not strive for the universally portable solution
 *       in binary floating point serialization as desired by some boost users.
 *       Instead we support only the most widely used IEEE 754 format and try to
 *       detect when requirements are not met and hence our approach must fail.
 *       Contributions we made by Johan Rade and Ákos Maróy.
 *
 * \note Version 2.0 fixes a serious bug that effectively transformed most
 *       of negative integral values into positive values! For example the two
 *       numbers -12 and 234 were stored in the same 8-bit pattern and later
 *       always restored to 234. This was fixed in this version in a way that
 *       does not change the interpretation of existing archives that did work
 *       because there were no negative numbers. The other way round archives
 *       created by version 2.0 and containing negative numbers will raise an
 *       integer type size exception when reading it with version 1.0. Thanks
 *       to Markus Frohnmaier for testing the archives and finding the bug.
 *
 * \copyright The boost software license applies.
 */
/*****************************************************************************/

#pragma once

// endian and fpclassify
// disable std::fpclassify because it lacks the fp_traits::bits classes
#define BOOST_MATH_DISABLE_STD_FPCLASSIFY
#include <boost/endian/conversion.hpp>
#include <boost/math/special_functions/fpclassify.hpp>

// generic type traits for numeric types
#include "portable_archive_exception.hpp"
#include <cstdint>
#include <type_traits>

namespace lsl {
/*template<typename T>
int fpclassify(T val) { return std::fpclassify(val); }*/
namespace detail {

template <typename T> struct fp_traits_consts {};
template <> struct fp_traits_consts<double> {
	using bits = uint64_t;
	static constexpr bits sign = (0x80000000ull) << 32, exponent = (0x7ff00000ll) << 32,
						  significand = ((0x000fffffll) << 32) + (0xfffffffful);
};
template <> struct fp_traits_consts<float> {
	using bits = uint32_t;
	static constexpr bits sign = 0x80000000u, exponent = 0x7f800000, significand = 0x007fffff;
	static void get_bits(float src, bits &dst) { std::memcpy(&dst, &src, sizeof(src)); }
	static void set_bits(float &dst, bits src) { std::memcpy(&dst, &src, sizeof(src)); }
};

template <typename T> struct fp_traits {
	using type = fp_traits_consts<T>;
	static void get_bits(double src, typename type::bits &dst) {
		std::memcpy(&dst, &src, sizeof(src));
	}
	static void set_bits(double &dst, typename type::bits src) {
		std::memcpy(&dst, &src, sizeof(src));
	}
};

} // namespace detail
} // namespace lsl

namespace fp = lslboost::math;
// namespace alias endian
namespace endian = lslboost::endian;

// hint from Johan Rade: on VMS there is still support for
// the VAX floating point format and this macro detects it
#if defined(__vms) && defined(__DECCXX) && !__IEEE_FLOAT
#error "VAX floating point format is not supported!"
#endif
