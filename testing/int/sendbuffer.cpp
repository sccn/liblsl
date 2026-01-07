/**
 * Tests for consumer_queue refcount corruption bug.
 *
 * These tests demonstrate the bug in consumer_queue::move_or_drop where
 * explicit destructor call causes double-release of samples, corrupting
 * the reference count when samples are shared across multiple queues.
 *
 * The bug mechanism:
 * 1. Sample pushed to multiple queues via send_buffer (refcount = N)
 * 2. One queue overflows, calls move_or_drop(slot.value)
 * 3. Explicit destructor decrements refcount (N -> N-1)
 * 4. slot.value.px still contains old pointer (garbage, not zeroed)
 * 5. try_push assigns new sample, assignment's temp destructor
 *    calls intrusive_ptr_release on garbage pointer
 * 6. This decrements refcount AGAIN (N-1 -> N-2)
 *
 * Each overflow event decrements refcount twice instead of once!
 * This causes refcount corruption that eventually leads to crashes.
 *
 * Key insight: The bug requires SHARED samples via send_buffer.
 * Single-queue tests don't trigger visible crashes because samples
 * have refcount=1, and double-release just corrupts a single sample.
 *
 * Related: PR #246, commit 11b6207e (2016 fix for cf_string double delete)
 */

#include "consumer_queue.h"
#include "sample.h"
#include "send_buffer.h"
#include <atomic>
#include <catch2/catch_all.hpp>
#include <thread>
#include <vector>

// clazy:excludeall=non-pod-global-static

/**
 * Test: Rapid queue create-destroy with send_buffer
 *
 * It simulates the scenario where inlets connect and disconnect rapidly
 * while samples are being pushed through send_buffer. The key elements:
 *
 * 1. Uses send_buffer to share samples across multiple queues
 * 2. Small buffer size forces frequent overflow (triggers move_or_drop)
 * 3. Rapid create/destroy cycles accumulate refcount corruption
 * 4. Queue destruction with corrupted refcounts causes crash
 */
TEST_CASE("rapid queue lifecycle with send_buffer", "[queue][regression][send_buffer]") {
	const int buffer_size = 4;
	const int num_iterations = 50;

	lsl::factory fac(lsl_channel_format_t::cft_int32, 2, buffer_size);
	auto sendbuf = std::make_shared<lsl::send_buffer>(buffer_size);

	for (int iter = 0; iter < num_iterations; ++iter) {
		// Create queues that will share samples via send_buffer
		std::vector<std::shared_ptr<lsl::consumer_queue>> queues;
		for (int i = 0; i < 5; ++i) {
			queues.push_back(sendbuf->new_consumer(buffer_size));
		}

		// Push samples through send_buffer - each sample goes to ALL 5 queues
		// This means each sample has refcount = 5
		// Buffer overflow causes move_or_drop which double-releases
		for (int i = 0; i < buffer_size * 3; ++i) {
			auto sample = fac.new_sample(static_cast<double>(i), true);
			sendbuf->push_sample(sample);
		}

		// Push null sample (the PR #246 scenario)
		sendbuf->push_sample(lsl::sample_p());

		// Queues are destroyed here when vector goes out of scope
		// Corrupted refcounts cause crash during cleanup
	}

	REQUIRE(true);
}

/**
 * Test: String sample overflow with send_buffer
 *
 * String samples were specifically mentioned in commit 11b6207e (2016):
 * "Fixed double pointer delete error when destroying cf_string stream"
 *
 * String samples have embedded std::string objects that require proper
 * lifetime management. This test verifies the bug affects string samples
 * when shared via send_buffer.
 */
TEST_CASE("string sample send_buffer overflow", "[queue][regression][send_buffer][string]") {
	const int buffer_size = 4;
	const int num_queues = 5;
	const int num_samples = 50;
	const int num_channels = 2;

	lsl::factory fac(lsl_channel_format_t::cft_string, num_channels, buffer_size);
	auto sendbuf = std::make_shared<lsl::send_buffer>(buffer_size);

	std::vector<std::shared_ptr<lsl::consumer_queue>> queues;
	for (int i = 0; i < num_queues; ++i) {
		queues.push_back(sendbuf->new_consumer(buffer_size));
	}

	// Push string samples - shared across all queues
	for (int i = 0; i < num_samples; ++i) {
		auto sample = fac.new_sample(static_cast<double>(i), true);
		std::string data[num_channels];
		data[0] = "test_" + std::to_string(i);
		data[1] = "string_data_" + std::to_string(i * 100);
		sample->assign_typed(data);
		sendbuf->push_sample(sample);
	}

	// Push null (PR #246 scenario)
	sendbuf->push_sample(lsl::sample_p());

	// Flush and verify no crash
	for (auto &q : queues) {
		q->flush();
	}

	REQUIRE(true);
}

/**
 * Test: Multi-threaded send_buffer stress test
 *
 * Multiple threads push to send_buffer concurrently to maximize
 * race condition probability. This tests the concurrent scenario
 * that occurs in real-world usage with multiple outlet threads.
 */
TEST_CASE("multi-threaded send_buffer stress", "[queue][regression][send_buffer][concurrent]") {
	const int buffer_size = 4;
	const int num_queues = 5;
	const int num_producer_threads = 4;
	const int iterations_per_thread = 200;

	lsl::factory fac(lsl_channel_format_t::cft_float32, 4, buffer_size * 2);
	auto sendbuf = std::make_shared<lsl::send_buffer>(buffer_size);

	std::vector<std::shared_ptr<lsl::consumer_queue>> queues;
	for (int i = 0; i < num_queues; ++i) {
		queues.push_back(sendbuf->new_consumer(buffer_size));
	}

	std::atomic<int> push_count{0};

	// Producer threads push samples concurrently
	auto producer = [&]() {
		for (int i = 0; i < iterations_per_thread; ++i) {
			auto sample = fac.new_sample(static_cast<double>(i), true);
			sendbuf->push_sample(sample);
			push_count.fetch_add(1, std::memory_order_relaxed);
		}
	};

	// Start producer threads
	std::vector<std::thread> threads;
	for (int i = 0; i < num_producer_threads; ++i) {
		threads.emplace_back(producer);
	}

	// Wait for all producers to finish
	for (auto &t : threads) {
		t.join();
	}

	// Push null to signal end
	sendbuf->push_sample(lsl::sample_p());

	// Flush all queues
	for (auto &q : queues) {
		q->flush();
	}

	INFO("Pushed " << push_count.load() << " samples");
	REQUIRE(push_count.load() == num_producer_threads * iterations_per_thread);
}
