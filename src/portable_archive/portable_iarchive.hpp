#pragma once

#include <istream>
#include "portable_archive_includes.hpp"

#ifdef SLIMARCHIVE
#include "slimarchive.hpp"
#else
#include <boost/archive/basic_binary_iprimitive.hpp>
#include <boost/archive/basic_binary_iarchive.hpp>

#include <boost/archive/detail/polymorphic_iarchive_route.hpp>
#endif


namespace eos {

	// forward declaration
	class portable_iarchive;

	using portable_iprimitive = lslboost::archive::basic_binary_iprimitive<portable_iarchive,
		std::istream::char_type, std::istream::traits_type>;

	/**
	 * \brief Portable binary input archive using little endian format.
	 *
	 * This archive addresses integer size, endianness and floating point types so
	 * that data can be transferred across different systems. There may still be
	 * constraints as to what systems are compatible and the user will have to take
	 * care that e.g. a very large int being saved on a 64 bit machine will result
	 * in a portable_archive_exception if loaded into an int on a 32 bit system.
	 * A possible workaround to this would be to use fixed types like
	 * lslboost::uint64_t in your serialization structures.
	 *
	 * \note The class is based on the portable binary example by Robert Ramey and
	 *	     uses Beman Dawes endian library plus fp_utilities by Johan Rade.
	 */
	class portable_iarchive : public portable_iprimitive

		// the example derives from common_oarchive but that lacks the
		// load_override functions so we chose to stay one level higher
		, public lslboost::archive::basic_binary_iarchive<portable_iarchive>
	{
		// only needed for Robert's hack in basic_binary_iarchive::init
		friend class lslboost::archive::basic_binary_iarchive<portable_iarchive>;

		// workaround for gcc: use a dummy struct
		// as additional argument type for overloading
		template <int> struct dummy {
			dummy(int /*unused*/) {}
		};

		// loads directly from stream
		inline signed char load_signed_char()
		{ 
			signed char c; 
			portable_iprimitive::load(c); 
			return c; 
		}

		// archive initialization
		void init(unsigned flags)
		{
			using namespace lslboost::archive;
			archive_version_type input_library_version(3);

			// it is vital to have version information!
			// if we don't have any we assume boost 1.33
			if (flags & no_header)
				set_library_version(input_library_version);

			// extract and check the magic eos byte
			else if (load_signed_char() != magic_byte)
				throw archive_exception(archive_exception::invalid_signature);

			else
			{
				// extract version information
				operator>>(input_library_version);

				// throw if file version is newer than we are
				if (input_library_version > archive_version)
					throw archive_exception(archive_exception::unsupported_version);

				// else set the library version accordingly
				else set_library_version(input_library_version);
			}
		}

	public:
		/**
		 * \brief Constructor on a stream using ios::binary mode!
		 *
		 * We cannot call basic_binary_iprimitive::init which tries to detect
		 * if the binary archive stems from a different platform by examining
		 * type sizes.
		 *
		 * We could have called basic_binary_iarchive::init which would create
		 * the lslboost::serialization standard archive header containing also the
		 * library version. Due to efficiency we stick with our own.
		 */
		portable_iarchive(std::istream& is, unsigned flags = 0)
			: portable_iprimitive(*is.rdbuf(), flags & lslboost::archive::no_codecvt)
			, lslboost::archive::basic_binary_iarchive<portable_iarchive>(flags)
		{
			init(flags);
		}

		portable_iarchive(std::streambuf& sb, unsigned flags = 0)
			: portable_iprimitive(sb, flags & lslboost::archive::no_codecvt)
			, lslboost::archive::basic_binary_iarchive<portable_iarchive>(flags)
		{
			init(flags);
		}

		//! Load narrow strings.
		void load(std::string& s) 
		{
			portable_iprimitive::load(s);
		}

        /**
         * \brief Loading bool type.
         *
         * Byte pattern is same as with integer types, so this function
         * is somewhat redundant but treating bool as integer generates
		 * a lot of compiler warnings.
         *
         * \note If you cannot compile your application and it says something
         * about load(bool) cannot convert your type A& into bool& then you
         * should check your BOOST_CLASS_IMPLEMENTATION setting for A, as
         * portable_archive is not able to handle custom primitive types in
         * a general manner.
         */
		void load(bool& b) 
		{ 
			switch (signed char c = load_signed_char())
			{
			case 0: b = false; break;
			case 1: b = load_signed_char(); break;
			default: throw portable_archive_exception(c);
			}
		}

		/**
		 * \brief Load integer types.
		 *
		 * First we load the size information ie. the number of bytes that 
		 * hold the actual data. Then we retrieve the data and transform it
		 * to the original value by using load_little_endian.
		 */
		template <typename T>
		typename std::enable_if<std::is_integral<T>::value >::type
		load(T & t, dummy<2> = 0)
		{
			// get the number of bytes in the stream
			if (signed char size = load_signed_char())
			{
				// check for negative value in unsigned type
				if (size < 0 && std::is_unsigned<T>::value)
					throw portable_archive_exception();

				// check that our type T is large enough
				else if ((unsigned) abs(size) > sizeof(T)) 
					throw portable_archive_exception(size);

				// reconstruct the value
				T temp = size < 0 ? -1 : 0;
				load_binary(&temp, abs(size));

				// load the value from little endian - it is then converted
				// to the target type T and fits it because size <= sizeof(T)
				t = endian::native_to_little(temp);
			}

			else t = 0; // zero optimization
		}

		/** 
		 * \brief Load floating point types.
		 * 
		 * We simply rely on fp_traits to set the bit pattern from the (unsigned)
		 * integral type that was stored in the stream. Francois Mauger provided
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
		typename std::enable_if<std::is_floating_point<T>::value >::type
		load(T & t, dummy<3> = 0)
		{
			using traits = typename fp::detail::fp_traits<T>::type;

			// if you end here there are three possibilities:
			// 1. you're serializing a long double which is not portable
			// 2. you're serializing a double but have no 64 bit integer
			// 3. your machine is using an unknown floating point format
			// after reading the note above you still might decide to 
			// deactivate this static assert and try if it works out.
			typename traits::bits bits;
			BOOST_STATIC_ASSERT(sizeof(bits) == sizeof(T));
			BOOST_STATIC_ASSERT(std::numeric_limits<T>::is_iec559);

			load(bits);
			traits::set_bits(t, bits);

			// if the no_infnan flag is set we must throw here
			if (get_flags() & no_infnan && !fp::isfinite(t))
				throw portable_archive_exception(t);

			// if you end here your floating point type does not support 
			// denormalized numbers. this might be the case even though 
			// your type conforms to IEC 559 (and thus to IEEE 754)
			if (std::numeric_limits<T>::has_denorm == std::denorm_absent
				&& fp::fpclassify(t) == (int) FP_SUBNORMAL) // GCC4
				throw portable_archive_exception(t);
		}

		// in boost 1.44 version_type was splitted into library_version_type and
		// item_version_type, plus a whole bunch of additional strong typedefs.
		template <typename T>
		typename std::enable_if<!std::is_arithmetic<T>::value >::type
		load(T& t, dummy<4> = 0)
		{
			// we provide a generic load routine for all types that feature
			// conversion operators into an unsigned integer value like those
			// created through BOOST_STRONG_TYPEDEF(X, some unsigned int) like
			// library_version_type, collection_size_type, item_version_type,
			// class_id_type, object_id_type, version_type and tracking_type
			load((typename lslboost::uint_t<sizeof(T)*CHAR_BIT>::least&)(t));
		}
	};
} // namespace eos

// this is required by export which registers all of your
// classes with all the inbuilt archives plus our archive.
BOOST_SERIALIZATION_REGISTER_ARCHIVE(eos::portable_iarchive)

// if you include this header multiple times and your compiler is picky
// about multiple template instantiations (eg. gcc is) then you need to
// define NO_EXPLICIT_TEMPLATE_INSTANTIATION before every include but one
// or you move the instantiation section into an implementation file
#ifndef NO_EXPLICIT_TEMPLATE_INSTANTIATION

#include <boost/archive/impl/basic_binary_iarchive.ipp>
#include <boost/archive/impl/basic_binary_iprimitive.ipp>

#if !defined BOOST_ARCHIVE_SERIALIZER_INCLUDED
#include <boost/archive/impl/archive_serializer_map.ipp>
#define BOOST_ARCHIVE_SERIALIZER_INCLUDED
#endif

namespace lslboost { namespace archive {
	// explicitly instantiate for this type of binary stream
	template class basic_binary_iarchive<eos::portable_iarchive>;

	template class basic_binary_iprimitive<
		eos::portable_iarchive
		, std::istream::char_type
		, std::istream::traits_type
	>;

	template class detail::archive_serializer_map<eos::portable_iarchive>;
} } // namespace lslboost::archive

#endif
