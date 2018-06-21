#include "socket_utils.h"
#include <boost/endian/conversion.hpp>
#include "common.h"


// === Implementation of the socket utils ===

using namespace lsl;

/// Measure the endian conversion performance of this machine.
double lsl::measure_endian_performance() {
	const double measure_duration = 0.01;
	const double t_end=lsl_clock() + measure_duration;
	uint64_t data=0x01020304;
	double k;
	for (k=0; ((int)k & 0xFF) != 0 || lsl_clock()<t_end; k++)
		lslboost::endian::endian_reverse_inplace(data);
	return k;
}
