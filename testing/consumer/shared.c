/* A library that uses liblsl internally and exports only its own function. */
#include <lsl_c.h>

#if defined(_WIN32)
#define CONSUMER_API __declspec(dllexport)
#else
#define CONSUMER_API __attribute__((visibility("default")))
#endif

CONSUMER_API int consumer_shared_lsl_version(void) { return lsl_library_version(); }
