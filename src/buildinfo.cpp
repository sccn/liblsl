extern "C" {
#include "../include/lsl/common.h"

LIBLSL_C_API const char *lsl_library_info(void) {
#ifdef LSL_LIBRARY_INFO_STR
	return LSL_LIBRARY_INFO_STR;
#else
	return "Unknown (not set by build system)";
#endif
}
}
