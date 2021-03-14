#include "../src/consumer_queue.h"
#include "../src/sample.h"
#include <atomic>
#include <catch2/catch.hpp>
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
	const unsigned int size = 100000;
	lsl::factory fac(lsl_channel_format_t::cft_int8, 4, 1);
	auto sample = fac.new_sample(0.0, true);
	lsl::consumer_queue queue(size);
	std::atomic<bool> done{false};

	std::thread pusher([&]() {
		for (unsigned int i = 0; i < size; ++i) queue.push_sample(sample);
		done = true;
	});

	unsigned flushes = 0, pulled = 0;
	// Pull samples until the pusher is done and the queue is empty
	while (true) {
		unsigned int n = queue.flush();
		if (n) {
			flushes++;
			pulled += n;
		} else {
			if(done && queue.read_available() == 0) break;
			std::this_thread::yield();
		}
	}
	INFO(flushes)
	CHECK(pulled == size);
	pusher.join();
}
