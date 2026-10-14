/* Same as main.cpp, through the C API (a C program linking a C++ library). */
#include <lsl_c.h>
#include <stdio.h>
#include <string.h>

int main(void) {
	lsl_streaminfo info =
		lsl_create_streaminfo("consumer_c", "Test", 1, 100, cft_float32, "consumer_c_uid");
	lsl_outlet outlet = lsl_create_outlet(info, 0, 360);
	char *xml = lsl_get_xml(info);
	printf("liblsl %d, stream xml: %u bytes\n", lsl_library_version(), (unsigned)strlen(xml));
	lsl_destroy_string(xml);
	lsl_destroy_outlet(outlet);
	lsl_destroy_streaminfo(info);
	return 0;
}
