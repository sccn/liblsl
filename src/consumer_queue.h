#ifndef CONSUMER_QUEUE_H
#define CONSUMER_QUEUE_H

#include "common.h"
#include "sample.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace lsl {

// size of a cache line
#if defined(__s390__) || defined(__s390x__)
constexpr int CACHELINE_BYTES = 256;
#elif defined(powerpc) || defined(__powerpc__) || defined(__ppc__)
constexpr int CACHELINE_BYTES = 128;
#else
constexpr int CACHELINE_BYTES = 64;
#endif

constexpr int _min(int a, int b) { return a > b ? b : a; }

/// calculate the needed padding to separate two members E1 and E2 by at least CACHELINE_BYTES
template <typename E1, typename E2, typename... T> constexpr std::size_t padding() {
	return CACHELINE_BYTES -
		   _min(sizeof(std::tuple<E1, T..., E2>) - sizeof(E1) - sizeof(E2), CACHELINE_BYTES - 1);
}

template <typename E1, typename E2, typename... T> struct Padding {
	const char pad[padding<E1, E2, T...>()]{0};
};

/**
 * A thread-safe producer/consumer queue of unread samples.
 *
 * Erases the oldest samples if max capacity is exceeded. Implemented as a ring buffer (wait-free
 * unless the buffer is full or empty).
 */
class consumer_queue {
public:
	/**
	 * Create a new queue with a given capacity.
	 * @param size The maximum number of samples that can be held by the queue. Beyond that,
	 * the oldest samples are dropped.
	 * @param registry Optionally a pointer to a registration facility, for multiple-reader
	 * arrangements.
	 */
	explicit consumer_queue(std::size_t size, send_buffer_p registry = send_buffer_p());

	/// Destructor. Unregisters from the send buffer, if any.
	~consumer_queue();

	/**
	 * Push a new sample onto the queue. Can only be called by one thread (single-producer).
	 * This deletes the oldest sample if the max capacity is exceeded.
	 */
	template <class T> void push_sample(T &&sample) {
		while (!try_push(std::forward<T>(sample))) {
			// buffer full, drop oldest sample
			if (!done_sync_.load(std::memory_order_acquire)) {
				// synchronizes-with store to done_sync_ in ctor
				std::atomic_thread_fence(std::memory_order_acquire);
				done_sync_.store(true, std::memory_order_release);
			}
			try_pop();
		}
		{
			// ensure that notify_one doesn't happen in between try_pop and wait_for
			std::lock_guard<std::mutex> lk(mut_);
			cv_.notify_one();
		}
	}

	/**
	 * Pop a sample from the queue. Can be called by multiple threads (multi-consumer).
	 * Blocks if empty and if a nonzero timeout is used.
	 * @param timeout Timeout for the blocking, in seconds. If expired, an empty sample is returned.
	 */
	sample_p pop_sample(double timeout = FOREVER) {
		sample_p result;
		bool success = try_pop(result);
		if (!success && timeout > 0.0) {
			// only acquire mutex if we have to do a blocking wait with timeout
			std::chrono::duration<double> sec(timeout);
			std::unique_lock<std::mutex> lk(mut_);
			if (!try_pop(result)) cv_.wait_for(lk, sec, [&] { return this->try_pop(result); });
		}
		return result;
	}

	/// Number of available samples. This is approximate unless called by the thread calling the
	/// pop_sample().
	std::size_t read_available() const;

	/// Flush the queue, return the number of dropped samples.
	uint32_t flush() noexcept;

	/// Check whether the buffer is empty. This is approximate unless called by the thread calling
	/// the pop_sample().
	bool empty() const;

	consumer_queue(const consumer_queue&) = delete;
	consumer_queue(consumer_queue &&) = delete;
	consumer_queue& operator=(const consumer_queue&) = delete;
	consumer_queue &operator=(consumer_queue &&) = delete;

private:
	// an item stored in the queue
	struct item_t {
		std::atomic<std::size_t> seq_state;
		sample_p value;
	};

	// Push a new element to the queue.
	// Returns true if successful or false if queue full.
	template <class T> bool try_push(T &&sample) {
		std::size_t write_index = write_idx_.load(std::memory_order_acquire);
		std::size_t next_idx = add1_wrap(write_index);
		item_t &item = buffer_[write_index % size_];
		if (UNLIKELY(write_index != item.seq_state.load(std::memory_order_acquire)))
			return false; // item currently occupied, queue full
		write_idx_.store(next_idx, std::memory_order_release);
		copy_or_move(item.value, std::forward<T>(sample));
		item.seq_state.store(next_idx, std::memory_order_release);
		return true;
	}

	// Pop an element from the queue (can be called with zero or one argument). Returns true if
	// successful or false if queue is empty. Uses the same method as Vyukov's bounded MPMC queue.
	template <class... T> bool try_pop(T &...result) {
		item_t *item;
		std::size_t read_index = read_idx_.load(std::memory_order_relaxed);
		for (;;) {
			item = &buffer_[read_index % size_];
			const std::size_t seq_state = item->seq_state.load(std::memory_order_acquire);
			const std::size_t next_idx = add1_wrap(read_index);
			// check if the item is ok to pop
			if (LIKELY(seq_state == next_idx)) {
				// yes, try to claim slot using CAS
				if (LIKELY(read_idx_.compare_exchange_weak(
						read_index, next_idx, std::memory_order_relaxed)))
					break;
			} else if (LIKELY(seq_state == read_index))
				return false; // queue empty
			else
				// we're behind or ahead of another pop, try again
				read_index = read_idx_.load(std::memory_order_relaxed);
		}
		move_or_drop(item->value, result...);
		// mark item as free for next pass
		item->seq_state.store(add_wrap(read_index, size_), std::memory_order_release);
		return true;
	}

	// helper to either copy or move a value, depending on whether it's an rvalue ref
	inline static void copy_or_move(sample_p &dst, const sample_p &src) { dst = src; }
	inline static void copy_or_move(sample_p &dst, sample_p &&src) { dst = std::move(src); }
	// helper to either move or drop a value, depending on whether a dst argument is given
	inline static void move_or_drop(sample_p &src) { src.~sample_p(); }
	inline static void move_or_drop(sample_p &src, sample_p &dst) { dst = std::move(src); }

	/// helper to add a delta to the given index and wrap correctly
	inline std::size_t add_wrap(std::size_t x, std::size_t delta) const noexcept {
		const std::size_t xp = x + delta;
		return xp >= wrap_at_ ? xp - wrap_at_ : xp;
	}

	/// helper to increment the given index, wrapping it if necessary
	inline std::size_t add1_wrap(std::size_t x) const noexcept {
		return ++x == wrap_at_ ? 0 : x;
	}

	/// current read position
	std::atomic<std::size_t> read_idx_{0};
	/// condition for waiting with timeout
	std::condition_variable cv_;
	/// the sample buffer
	item_t *buffer_;

	/// padding to ensure read_idx_ and write_idx_ don't share a cacheline
	Padding<std::size_t, std::size_t, std::condition_variable, void *> pad;

	/// current write position
	std::atomic<std::size_t> write_idx_{0};
	/// max number of elements in the queue
	const std::size_t size_;
	/// threshold at which to wrap read/write indices
	const std::size_t wrap_at_;
	/// for use with the condition variable
	std::mutex mut_;

	/// optional consumer registry
	send_buffer_p registry_;

	/// padding to ensure write_ix_ and done_sync_ don't share a cacheline
#if UINTPTR_MAX <= 0xFFFFFFFF
	Padding<std::size_t, bool, std::size_t, std::size_t, std::mutex, send_buffer_p> pad2;
#endif

	/// whether we have performed a sync on the data stored by the constructor
	std::atomic<bool> done_sync_{false};
};

} // namespace lsl

#endif
