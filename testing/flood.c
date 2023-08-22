#include <lsl_c.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static double start_t;
static int64_t samples_pushed = 0;

void handle_signal(int signal) {
	double time = lsl_local_clock() - start_t;
	printf("%ld samples pushed in %fs, %d samples/s\n", samples_pushed, time,
		(int)(samples_pushed / time));
	if (signal == SIGTERM || signal == SIGINT) exit(0);
	start_t = lsl_local_clock();
	samples_pushed = 0;
}

int main(int argc, char *argv[]) {
	signal(SIGTERM, handle_signal);
	signal(SIGINT, handle_signal);
#ifdef SIGUSR1
	signal(SIGUSR1, handle_signal);
#endif
#ifdef SIGCONT
	signal(SIGCONT, handle_signal);
#endif
	printf("LSL inlet stress tester. Sends [nchan] uint16 channels as fast as possible.\n");
	printf("Usage: %s [streamname] [nchan=56] [chunksize=500]\n", argv[0]);
	printf("Using lsl %d, lsl_library_info: %s\n\n", lsl_library_version(), lsl_library_info());

	const char *name = argc > 1 ? argv[1] : "Flood";
	const int nchan = argc > 2 ? strtol(argv[2], NULL, 10) : 56;
	const int samples_per_chunk = argc > 3 ? strtol(argv[3], NULL, 10) : 500;
	const char uid[] = "325wqer4354";

	/* declare a new streaminfo (name: SendDataC / argument 1, content type: EEG, 8 channels, 500
	 * Hz, float values, some made-up device id (can also be empty) */
	lsl_streaminfo info = lsl_create_streaminfo(name, "Bench", nchan, 50000, cft_int16, uid);

	/* make a new outlet (chunking: default, buffering: 360 seconds) */
	lsl_outlet outlet = lsl_create_outlet_ex(info, 0, 360, transp_sync_blocking);
	char *infoxml = lsl_get_xml(lsl_get_info(outlet));
	printf("Streaminfo: %s\n", infoxml);
	lsl_destroy_string(infoxml);

	const int buf_elements = nchan * samples_per_chunk;
	int16_t *buf = malloc(buf_elements * 2);
	memset(buf, 0xab, buf_elements * sizeof(buf[0]));

	printf("Now sending data...\n");
	start_t = lsl_local_clock();

	for (int t = 0;; t++) {
		lsl_push_chunk_s(outlet, buf, buf_elements);
		samples_pushed += samples_per_chunk;
	}
	lsl_destroy_outlet(outlet);
	return 0;
}
