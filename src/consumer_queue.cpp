#include "consumer_queue.h"
#include "common.h"
#include "sample.h"
#include "send_buffer.h"
#include <chrono>
#include <thread>

using namespace lsl;

consumer_queue::consumer_queue(std::size_t max_capacity, send_buffer_p registry)
	: registry_(registry), buffer_(max_capacity) {
	if (registry_) registry_->register_consumer(this);
}

consumer_queue::~consumer_queue() {
	try {
		if (registry_) registry_->unregister_consumer(this);
	} catch (std::exception &e) {
		LOG_F(ERROR,
			"Unexpected error while trying to unregister a consumer queue from its registry: %s",
			e.what());
	}
}

void consumer_queue::push_sample(const sample_p &sample) {
	while (!buffer_.push(sample)) {
		sample_p dummy;
		buffer_.pop(dummy);
	}
}

sample_p consumer_queue::pop_sample(double timeout) {
	sample_p result;
	if (timeout <= 0.0) {
		buffer_.pop(result);
	} else {
		if (!buffer_.pop(result)) {
			// turn timeout into the point in time at which we give up
			timeout += lsl::lsl_clock();
			do {
				if (lsl::lsl_clock() >= timeout) break;
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			} while (!buffer_.pop(result));
		}
	}
	return result;
}

uint32_t consumer_queue::flush() noexcept {
	uint32_t n = 0;
	while (buffer_.pop()) n++;
	return n;
}

bool consumer_queue::empty() { return buffer_.empty(); }
