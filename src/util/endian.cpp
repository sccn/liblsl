#include "endian.hpp"
#include "../common.h"

double lsl::measure_endian_performance() {
	const double measure_duration = 0.01;
	const double t_end = lsl_clock() + measure_duration;
	uint64_t data = 0x01020304;
	double k;
	for (k = 0; ((int)k & 0xFF) != 0 || lsl_clock() < t_end; k++)
		lslboost::endian::endian_reverse_inplace(data);
	return k;
}
