#pragma once

#include <boost/endian/conversion.hpp>

namespace lsl {

using lslboost::endian::endian_reverse_inplace;
using byteorder = lslboost::endian::order;

enum Endianness {
	LSL_PORTABLE_ENDIAN = 0,
	LSL_LITTLE_ENDIAN_BUT_BIG_FLOAT = 1,
	LSL_BIG_ENDIAN_BUT_LITTLE_FLOAT = 2,
	LSL_LITTLE_ENDIAN = 1234,
	LSL_BIG_ENDIAN = 4321,
	LSL_PDP11 = 2134 // deprecated, not used nowadays
};

/// the host native byte order
const Endianness LSL_BYTE_ORDER =
	(byteorder::native == byteorder::little) ? LSL_LITTLE_ENDIAN : LSL_BIG_ENDIAN;

inline bool can_convert_endian(Endianness requested, int value_size) {
	if(value_size == 1) return true;
	if (requested != LSL_LITTLE_ENDIAN && requested != LSL_BIG_ENDIAN)
		return false;
	if (LSL_BYTE_ORDER != LSL_LITTLE_ENDIAN && LSL_BYTE_ORDER != LSL_BIG_ENDIAN)
		return false;
	return true;
}
inline bool can_convert_endian(int requested, int value_size) {
	return can_convert_endian(static_cast<Endianness>(requested), value_size);
}

/// Measure the endian conversion performance of this machine.
double measure_endian_performance();

// Determine target byte order / endianness
using byteorder = lslboost::endian::order;
static_assert(byteorder::native == byteorder::little || byteorder::native == byteorder::big,
	"Unsupported byteorder");


} // namespace lsl
