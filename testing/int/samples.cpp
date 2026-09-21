#include "../src/consumer_queue.h"
#include "../src/sample.h"
#include <atomic>
#include <catch2/catch_all.hpp>
#include <thread>

// clazy:excludeall=non-pod-global-static

TEST_CASE("consumer_queue", "[queue][basic]") {
	const int size = 10;
	lsl::factory fac(lsl_channel_format_t::cft_int8, 4, size / 2);
	lsl::consumer_queue queue(size);
	for (int i = 0; i <= size; ++i) queue.push_sample(fac.new_sample(i, true));

	// Does the queue respect the capacity?
	CHECK(queue.read_available() == size);

	// Are the right samples dropped when going over capacity?
	CHECK(static_cast<int>(queue.pop_sample()->timestamp()) == 1);

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
	INFO(flushes);
	CHECK(pulled == size);
	pusher.join();
}

TEST_CASE("sample conversion", "[basic]") {
	lsl::factory fac(lsl_channel_format_t::cft_int64, 2, 1);
	double values[2] = {1, -1};
	int64_t buf[2];
	std::string strbuf[2];
	for (int i = 0; i < 30; ++i) {
		auto sample = fac.new_sample(0.0, true);
		sample->assign_typed(values);
		sample->retrieve_untyped(buf);
		sample->retrieve_typed(strbuf);
		for (int j = 0; j < 1; ++j) {
			CHECK(values[j] == static_cast<int64_t>(buf[j]));
			CHECK(strbuf[j] == std::to_string(buf[j]));
		}
		values[0] = (double)(buf[0] << 1);
		values[1] = (double)(-buf[0]);
	}
}

TEST_CASE("sample recycling acquires earlier readers", "[sample][threads]") {
	lsl::factory fac(cft_int32, 1, 1);
	for (int i = 0; i < 32; ++i) {
		auto sample = fac.new_sample(42.0, false);
		auto *address = sample.get();
		std::atomic<bool> released{false};
		double observed = 0.0;
		std::thread reader([copy = sample, &released, &observed]() mutable {
			observed = copy->timestamp();
			copy.reset();
			released.store(true, std::memory_order_relaxed);
		});
		struct join_thread {
			std::thread &thread;
			~join_thread() { if (thread.joinable()) thread.join(); }
		} joiner{reader};

		// Only control the release order here. An acquire/release handshake or
		// joining before reuse would hide synchronization missing from refcount_.
		while (!released.load(std::memory_order_relaxed)) std::this_thread::yield();
		sample.reset(); // Last owner must acquire the earlier reader's release.
		auto recycled = fac.new_sample(84.0, true);
		reader.join();
		CHECK(recycled.get() == address);
		CHECK(observed == 42.0);
		CHECK(recycled->timestamp() == 84.0);
	}
}
