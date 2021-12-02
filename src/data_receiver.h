#ifndef DATA_RECEIVER_H
#define DATA_RECEIVER_H

#include "cancellation.h"
#include "common.h"
#include "consumer_queue.h"
#include "forward.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace lsl {

class inlet_connection; // Forward declaration

/** Internal class of an inlet that's retrieving the data (the samples) of the inlet.
 *
 * The actual communication runs in an internal background thread, while the public functions
 * (pull_sample_typed/untyped, open_stream, close_stream) wait for the thread to finish.
 * The public functions have an optional timeout after which they give up, while the background
 * thread continues to do its job (so the next public-function call may succeed within the timeout).
 * The background thread terminates only if the data_receiver is destroyed or the underlying
 * connection is lost or shut down.
 */
class data_receiver final : public cancellable_registry {
public:
	/**
	 * Construct a new data receiver from an info connection.
	 * @param conn An inlet connection object.
	 * @param max_buflen Optionally the maximum amount of data to buffer in samples (per-channel).
	 * Recording applications want to use a fairly large buffer size here, while real-time
	 * applications want to only buffer as much as they need to perform their next calculation.
	 * @param max_chunklen Optionally the maximum size, in samples, at which chunks are transmitted
	 * (the default corresponds to the chunk sizes used by the sender). Recording applications can
	 * use a generous size here (leaving it to the network how to pack things), while real-time
	 * applications may want a finer (perhaps 1-sample) granularity.
	 */
	data_receiver(inlet_connection &conn, int max_buflen = 360, int max_chunklen = 0);

	/// Destructor. Stops the background activities.
	~data_receiver();

	/**
	 * Open a new data stream.
	 * All samples pushed in at the other end from this moment onwards will be queued and
	 * eventually be delivered in response to pull_sample() or pull_chunk() calls.
	 * A pull call without preceding open_stream() serves as an implicit open_stream().
	 * @param timeout Optional timeout of the operation (default: no timeout).
	 * @throws lsl::timeout_error (if the timeout expires), or lsl::lost_error (if the stream
	 * source has been lost).
	 */
	void open_stream(double timeout = FOREVER);

	/**
	 * Close the current data stream.
	 * All samples still buffered or in flight will be dropped and the source will halt its
	 * buffering of data for this inlet. If an application stops being interested in data from a
	 * source (temporarily or not), it should call close_stream() to not pressure the source outlet
	 * to buffer unnecessarily large amounts of data (perhaps even running out of memory).
	 */
	void close_stream();

	/// Retrieve a sample from the sample queue and assign its contents to the given typed buffer.
	template <class T>
	double pull_sample_typed(T *buffer, uint32_t buffer_elements, double timeout = FOREVER);

	/// Read sample from the inlet and read it into a pointer to raw data.
	double pull_sample_untyped(void *buffer, int buffer_bytes, double timeout = FOREVER);

	/// Check whether the underlying buffer is empty. This value may be inaccurate.
	bool empty() { return sample_queue_.empty(); }

	std::size_t samples_available() { return sample_queue_.read_available(); }

	/// Flush the queue, return the number of dropped samples
	uint32_t flush() noexcept { return sample_queue_.flush(); }

private:
	/// The data reader thread.
	void data_thread();

	sample_p try_get_next_sample(double timeout);

	/// the underlying connection
	inlet_connection &conn_;

	// fields related to the data reader thread
	/// a factory to create samples of appropriate type
	factory_p sample_factory_;
	/// background read thread
	std::thread data_thread_;
	/// whether we need to check whether the thread has been started
	bool check_thread_start_;
	/// indicates to the data thread that it a close has been requested
	std::atomic<bool> closing_stream_;
	/// whether the stream has been connected / opened
	bool connected_;
	/// queue of samples ready to be picked up (populated by the data thread)
	consumer_queue sample_queue_;
	/// mutex to protect the connected state
	std::mutex connected_mut_;
	/// condition variable to indicate that an update for the connected state is available
	std::condition_variable connected_upd_;

	// internal data used by the reader thread
	/// the maximum number of samples to be buffered for this inlet
	int max_buflen_;
	// the desired maximum chunklen for received samples
	int max_chunklen_;
};

} // namespace lsl

#endif
