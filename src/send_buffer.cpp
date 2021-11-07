#include "send_buffer.h"
#include "consumer_queue.h"
#include <algorithm>
#include <chrono>
#include <loguru.hpp>
#include <memory>

using namespace lsl;

std::shared_ptr<consumer_queue> send_buffer::new_consumer(int max_buffered) {
	max_buffered = max_buffered ? std::min(max_buffered, max_capacity_) : max_capacity_;
	return std::make_shared<consumer_queue>(max_buffered, shared_from_this());
}


/**
 * Push a sample onto the send buffer.
 * Will subsequently be seen by all consumers.
 */
void send_buffer::push_sample(const sample_p &s) {
	std::lock_guard<std::mutex> lock(consumers_mut_);
	for (auto &consumer : consumers_) consumer->push_sample(s);
}


/// Registered a new consumer.
void send_buffer::register_consumer(consumer_queue *q) {
	{
		std::lock_guard<std::mutex> lock(consumers_mut_);
		if (std::find(consumers_.begin(), consumers_.end(), q) != consumers_.end())
			LOG_F(WARNING, "Duplicate consumer queue in send buffer");
		else
			consumers_.push_back(q);
	}
	some_registered_.notify_all();
}

/// Unregister a previously registered consumer.
void send_buffer::unregister_consumer(consumer_queue *q) {
	std::lock_guard<std::mutex> lock(consumers_mut_);
	auto pos = std::find(consumers_.begin(), consumers_.end(), q);
	if (pos == consumers_.end()) LOG_F(ERROR, "Trying to remove consumer queue not in send buffer");

	// Put the element to be removed at the end (if it isn't there already) and
	// remove the last element
	if (*pos != consumers_.back()) std::swap(*pos, consumers_.back());
	consumers_.pop_back();
}

/// Check whether there currently are consumers.
bool send_buffer::have_consumers() {
	std::lock_guard<std::mutex> lock(consumers_mut_);
	return some_registered();
}

/// Wait until some consumers are present.
bool send_buffer::wait_for_consumers(double timeout) {
	std::unique_lock<std::mutex> lock(consumers_mut_);
	return some_registered_.wait_for(
		lock, std::chrono::duration<double>(timeout), [this]() { return some_registered(); });
}
