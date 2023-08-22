#include <inttypes.h>
#include <lsl_c.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * This example program pushes a 16-bit stream with a constant value across all channels. The
 * channel number and rate are configurable via command line arguments. The data are not copied!
 * This requires an inconvenient application design using threads and synchronization, but the
 * this kind of design is necessary when frequent copying of large data chunks is too expensive.
 * The main thread takes data from the device and puts it into a transmit buffer, and the secondary
 * thread pushes the data from the transmit buffer into the lsl outlet then releases the data from
 * the buffer. This is a relatively new and unusual design pattern for lsl. It uses a new BLOCKING
 * function `push_chunk_wait`. Presently this only supports int16 types. This function passes
 * the data pointer directly to asio to write data to each consumer and waits for asio to return.
 * Note: One would usually prefer to use C++ niceities and queueing libraries (e.g.,
 * moodycamel::concurrentqueue) when working with big data across threads. However, the high-
 * throughput data demands might be come from embedded devices with compiler limitations,
 * so we use C here. Any input on how to speed up this C code is greatly appreciated.
 */

#define DEVICE_BUFFER_SIZE 4194304

typedef struct {
	char name[100];
	char serial[100];
	double last_timestamp;
	int srate;
	int nchans;
	int write_idx;
	int read_idx;
	int min_frames_per_chunk;
	int16_t channel_data[DEVICE_BUFFER_SIZE / 2]; // (channels * samples) < 2 million
} fake_device;

typedef struct {
	int thread_status;
	int chunk_size;
	double buffer_dur;
	int do_async;
} thread_params;

// Linked-list queue
typedef struct chunk_info {
	int16_t *buf;
	int n_frames;
	double timestamp;
} chunk_info;
typedef struct node {
	chunk_info info;
	struct node *next;
} node;
typedef struct queue {
	int count;
	node *front;
	node *rear;
} queue;
void initialize(queue *q) {
	q->count = 0;
	q->front = NULL;
	q->rear = NULL;
}
int isempty(queue *q) { return (q->front == NULL); }
void enqueue(queue *q, int16_t *data, int frames, double ts) {
	node *tmp;
	tmp = malloc(sizeof(node));
	tmp->info.buf = data;
	tmp->info.n_frames = frames;
	tmp->info.timestamp = ts;
	tmp->next = NULL;
	if (isempty(q))
		q->front = q->rear = tmp;
	else {
		q->rear->next = tmp;
		q->rear = tmp;
	}
	q->count++;
}
chunk_info dequeue(queue *q) {
	chunk_info info = q->front->info;
	node *tmp = q->front;
	free(tmp);
	q->front = q->front->next;
	q->count--;
	return (info);
}

// Globals
fake_device *device = 0;
sem_t sem;
queue *q;

// fetch_data -- Normally something provided by Device SDK
uint64_t fetch_data(int16_t **buffer) {
	static int buf_samples = sizeof(device->channel_data) / sizeof(device->channel_data[0]);

	if (device->last_timestamp < 0) device->last_timestamp = lsl_local_clock();
	double now = lsl_local_clock();
	// Calculate how many frames/timestamps have elapsed since the last call.
	uint64_t elapsed_frames = (uint64_t)((now - device->last_timestamp) * device->srate);
	if (elapsed_frames < device->min_frames_per_chunk) elapsed_frames = 0;
	// Cut this fetch short if it would go past the buffer. Next fetch will start at first idx.
	if ((device->write_idx + elapsed_frames * device->nchans) > buf_samples)
		elapsed_frames = (buf_samples - device->write_idx) / device->nchans;
	// Further limit elapsed_samples to not overtake the read point (tail)
	//	if ((device->write_idx < device->read_idx) &&
	//		(device->write_idx + (elapsed_frames * device->nchans) >= device->read_idx))
	//		elapsed_frames = (device->read_idx - device->write_idx) / device->nchans;
	if (elapsed_frames > 0) {
		// New elapsed_time after accounting for rounding to integer frames.
		device->last_timestamp += (double)(elapsed_frames) / device->srate;
		// I assume that the device has its own acquisition buffer and that it copies data
		//  to a separate data buffer for API purposes.
		//  We are using a model where the device SDK shares its buffer with the client application.
		//  This is a bit unusual but allows for fastest throughput.
		*buffer = &(device->channel_data[device->write_idx]);

		// And we advance the head for the next data transfer.
		device->write_idx = (device->write_idx + elapsed_frames * device->nchans) % buf_samples;
		if ((buf_samples - device->write_idx) < device->nchans) device->write_idx = 0;
	}
	return elapsed_frames;
}

// transmit_thread -- responsible for popping data off the queue and pushing it to LSL
void transmit_thread(void *vargp) {
	// Initialize thread-local variables
	thread_params *params = (thread_params *)vargp;

	/* declare a new streaminfo */
	lsl_streaminfo info = lsl_create_streaminfo(
		device->name, "TestLSL", device->nchans, device->srate, cft_int16, device->serial);

	/* add some meta-data fields to it */
	/* (for more standard fields, see https://github.com/sccn/xdf/wiki/Meta-Data) */
	lsl_xml_ptr desc = lsl_get_desc(info);
	lsl_append_child_value(desc, "manufacturer", "LSL");
	lsl_xml_ptr chns = lsl_append_child(desc, "channels");
	char chanlabel[20];
	for (int c = 0; c < device->nchans; c++) {
		lsl_xml_ptr chn = lsl_append_child(chns, "channel");
		snprintf(chanlabel, 20, "Chan-%d", c);
		lsl_append_child_value(chn, "label", chanlabel);
		lsl_append_child_value(chn, "unit", "microvolts");
		lsl_append_child_value(chn, "type", "EEG");
	}

	/* make a new outlet */
	lsl_outlet outlet =
		lsl_create_outlet_ex(info, params->chunk_size, params->buffer_dur, params->do_async ? transp_sync_blocking : 0);

	printf("Now sending data...\n");
	params->thread_status = 1;
	int buf_samples = sizeof(device->channel_data) / sizeof(device->channel_data[0]);
	while (params->thread_status) {
		sem_wait(&sem);
		if (!isempty(q)) {
			chunk_info chunk = dequeue(q);
			int64_t chunk_samples = chunk.n_frames * device->nchans;
			lsl_push_chunk_stp(outlet, chunk.buf, chunk_samples, chunk.timestamp, 1);
			device->read_idx = (device->read_idx + chunk_samples) % buf_samples;
		}
	}
	lsl_destroy_outlet(outlet);
}

int main(int argc, char *argv[]) {
	printf("SendDataCBlocking example program. Sends int16 data with minimal copies.\n");
	printf("Usage: %s [streamname] [streamuid] [srate] [nchans] [buff_dur] [do_async]\n", argv[0]);
	printf("Using lsl %d, lsl_library_info: %s\n", lsl_library_version(), lsl_library_info());
	const char *name = argc > 1 ? argv[1] : "SendDataCBlocking";
	const char *uid = argc > 2 ? argv[2] : "6s45ahas321";
	int srate = argc > 3 ? strtol(argv[3], NULL, 10) : 512;
	int n_chans = argc > 4 ? strtol(argv[4], NULL, 10) : 32;
	double buff_dur = argc > 5 ? strtod(argv[5], NULL) : 60.;
	int do_async = argc > 6 ? strtol(argv[6], NULL, 10) : 1;
	int32_t samps_per_chunk = argc > 6 ? strtol(argv[6], NULL, 10) : 30;

	// Initialize our fake device and set its parameters. This would normally be taken care of
	//  by the device SDK.
	device = (fake_device *)malloc(sizeof(fake_device));
	memset(device, 0, sizeof(fake_device));
	strcpy(device->name, name);
	device->srate = srate;
	device->nchans = n_chans;
	device->last_timestamp = -1.;
	device->min_frames_per_chunk = device->srate / 1000;
	strcpy(device->serial, uid);
	// Give the device buffer data some non-zero value.
	memset(device->channel_data, 23, sizeof(device->channel_data));
	// write_idx and read_idx are OK at 0.

	thread_params params;
	params.buffer_dur = buff_dur;
	params.chunk_size = samps_per_chunk;
	params.thread_status = 0;
	params.do_async = do_async;

	// Initialize q
	q = malloc(sizeof(queue));
	initialize(q);

	sem_init(&sem, 0, 0);
	pthread_t thread_id;
	if (pthread_create(&thread_id, NULL, (void *)&transmit_thread, &params)) {
		fprintf(stderr, "Error creating LSL transmit thread.\n");
		return 1;
	}

	int exit_condition = 0;
	int16_t *shared_buff;
	double last_timestamp_received = -1.;
	uint64_t n_frames_received = 0;
	while (1) {
		if (exit_condition) break;

		// Get data from device
		n_frames_received = fetch_data(&shared_buff);
		if (n_frames_received > 0) {
			enqueue(q, shared_buff, n_frames_received, device->last_timestamp);
			sem_post(&sem);
		}
	}

	if (params.thread_status) // Kill thread
	{
		params.thread_status = 0;
		if (pthread_join(thread_id, NULL)) {
			fprintf(stderr, "Error terminating LSL transmit thread.\n");
		}
	}
	sem_destroy(&sem);
	free(device);

	return 0;
}
