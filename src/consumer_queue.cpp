#include "consumer_queue.h"
#include "sample.h"
#include "send_buffer.h"
#include <chrono>

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
	// push a sample, dropping the oldest sample if the queue ist already full.
	// During this operation the producer becomes a second consumer, i.e., a case
	// where the underlying spsc queue isn't thread-safe) so the mutex is locked.
	std::lock_guard<std::mutex> lk(mut_);
	while (!buffer_.push(sample)) {
		buffer_.pop();
	}
	cv_.notify_one();
}

sample_p consumer_queue::pop_sample(double timeout) {
	sample_p result;
	if (timeout <= 0.0) {
		std::lock_guard<std::mutex> lk(mut_);
		buffer_.pop(result);
	} else {
		std::unique_lock<std::mutex> lk(mut_);
		if (!buffer_.pop(result)) {
			// wait for a new sample until the thread calling push_sample delivers one and sends a
			// notification, or until timeout
			std::chrono::duration<double> sec(timeout);
			cv_.wait_for(lk, sec, [&]{ return this->buffer_.pop(result); });
		}
	}
	return result;
}

uint32_t consumer_queue::flush() noexcept {
	std::lock_guard<std::mutex> lk(mut_);
	uint32_t n = 0;
	while (buffer_.pop()) n++;
	return n;
}

bool consumer_queue::empty() { return buffer_.empty(); }
