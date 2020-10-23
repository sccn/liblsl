#include "../src/consumer_queue.h"
#include "../src/sample.h"
#include "catch.hpp"
#include <atomic>
#include <thread>

TEST_CASE("consumer_queue", "[queue][basic]") {
	const int size = 10;
	lsl::factory fac(lsl_channel_format_t::cft_int8, 4, size / 2);
	lsl::consumer_queue queue(size);
	for (int i = 0; i <= size; ++i) queue.push_sample(fac.new_sample(i, true));

	// Does the queue respect the capacity?
	CHECK(queue.read_available() == size);

	// Are the right samples dropped when going over capacity?
	CHECK(static_cast<int>(queue.pop_sample()->timestamp) == 1);

	// Does flush() return the correct count?
	CHECK(queue.flush() == size - 1);

	// Is the queue empty after flush()ing?
	CHECK(queue.empty());
}

TEST_CASE("consumer_queue_threaded", "[queue][threads]") {
	const int size = 10000;
	lsl::factory fac(lsl_channel_format_t::cft_int8, 4, 1);
	auto sample = fac.new_sample(0.0, true);
	lsl::consumer_queue queue(size);
	std::atomic<bool> done{false};

	std::thread pusher([&]() {
		for (int i = 0; i < size; ++i) queue.push_sample(sample);
		INFO(queue.read_available());
		done = true;
	});

	unsigned int n = 0, total = 0;
	// Pull samples until the pusher is done and the queue is empty
	while ((n = queue.flush()) != 0 || !done) total += n;
	CHECK(total == size);
	pusher.join();
}
