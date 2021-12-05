#include "consumer_queue.h"
#include "common.h"
#include "send_buffer.h"
#include <chrono>
#include <loguru.hpp>
#include <utility>

using namespace lsl;

consumer_queue::consumer_queue(std::size_t size, send_buffer_p registry)
	: buffer_(new item_t[size]), size_(size),
	  // largest integer at which we can wrap correctly
	  wrap_at_(std::numeric_limits<std::size_t>::max() - size -
			   std::numeric_limits<std::size_t>::max() % size),
	  registry_(std::move(registry)) {
	assert(size_ > 1);
	for (std::size_t i = 0; i < size_; ++i)
		buffer_[i].seq_state.store(i, std::memory_order_release);
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
	delete[] buffer_;
}

uint32_t consumer_queue::flush() noexcept {
	uint32_t n = 0;
	while (try_pop()) n++;
	return n;
}

std::size_t consumer_queue::read_available() const {
	std::size_t write_index = write_idx_.load(std::memory_order_acquire);
	std::size_t read_index = read_idx_.load(std::memory_order_relaxed);
	if (write_index >= read_index) return write_index - read_index;
	const std::size_t ret = write_index + size_ - read_index;
	return ret;
}

bool consumer_queue::empty() const {
	return write_idx_.load(std::memory_order_acquire) == read_idx_.load(std::memory_order_relaxed);
}
