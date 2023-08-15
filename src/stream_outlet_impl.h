#ifndef STREAM_OUTLET_IMPL_H
#define STREAM_OUTLET_IMPL_H

#include "common.h"
#include "forward.h"
#include "stream_info_impl.h"
#include <cstdint>
#include <loguru.hpp>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using asio::ip::tcp;
using asio::ip::udp;

namespace lsl {

/// pointer to a thread
using thread_p = std::shared_ptr<std::thread>;

/**
 * A stream outlet.
 * Outlets are used to make streaming data (and the meta-data) available on the lab network.
 */
class stream_outlet_impl {
public:
	/**
	 * Establish a new stream outlet. This makes the stream discoverable.
	 * @param info The stream information to use for creating this stream stays constant over the
	 * lifetime of the outlet.
	 * @param chunk_size The preferred chunk size, in samples, at which data shall be transmitted
	 * over the network. Can be selectively overridden by the inlet. If 0 (=default), the chunk size
	 * is determined by the pushthrough flag in push_sample or push_chunk.
	 * @param requested_size The maximum number of seconds/samples buffered for unresponsive
	 * receivers. If more samples get pushed, the oldest will be dropped. The default is sufficient
	 * to hold a bit more than 15 minutes of data at 512Hz, while consuming not more than ca. 512MB
	 * of RAM. Depends on `flags` as calculated in `stream_info_impl::calc_transport_buf_samples()`
	 * @param flags Bitwise-OR'd flags from lsl_transport_options_t
	 */
	stream_outlet_impl(const stream_info_impl &info, int32_t chunk_size = 0,
		int32_t requested_bufsize = 900, lsl_transport_options_t flags = transp_default);

	/**
	 * Destructor.
	 * The stream will no longer be discoverable after destruction and all paired inlets will stop
	 * delivering data.
	 */
	~stream_outlet_impl();

	stream_outlet_impl(const stream_outlet_impl &) = delete;


	//
	// === Pushing a sample into the outlet. ===
	//

	/**
	 * Push a std vector of values as a sample into the outlet.
	 *
	 * Each entry in the vector corresponds to one channel.
	 * The function handles type checking & conversion.
	 * @param data A vector of values to push (one for each channel).
	 * @param timestamp Optionally the capture time of the sample, in agreement with local_clock();
	 * if omitted, the current time is assumed.
	 * @param pushthrough Whether to push the sample through to the receivers instead of buffering
	 * it into a chunk according to network speeds.
	 */
	void push_sample(
		const std::vector<float> &data, double timestamp = 0.0, bool pushthrough = true) {
		check_numchan((int32_t)data.size());
		enqueue(data.data(), timestamp, pushthrough);
	}
	void push_sample(
		const std::vector<double> &data, double timestamp = 0.0, bool pushthrough = true) {
		check_numchan((int32_t)data.size());
		enqueue(data.data(), timestamp, pushthrough);
	}
	void push_sample(
		const std::vector<int64_t> &data, double timestamp = 0.0, bool pushthrough = true) {
		check_numchan((int32_t)data.size());
		enqueue(data.data(), timestamp, pushthrough);
	}
	void push_sample(
		const std::vector<int32_t> &data, double timestamp = 0.0, bool pushthrough = true) {
		check_numchan((int32_t)data.size());
		enqueue(data.data(), timestamp, pushthrough);
	}
	void push_sample(
		const std::vector<int16_t> &data, double timestamp = 0.0, bool pushthrough = true) {
		check_numchan((int32_t)data.size());
		enqueue(data.data(), timestamp, pushthrough);
	}
	void push_sample(
		const std::vector<char> &data, double timestamp = 0.0, bool pushthrough = true) {
		check_numchan((int32_t)data.size());
		enqueue(data.data(), timestamp, pushthrough);
	}
	void push_sample(
		const std::vector<std::string> &data, double timestamp = 0.0, bool pushthrough = true) {
		check_numchan((int32_t)data.size());
		enqueue(data.data(), timestamp, pushthrough);
	}

	/**
	 * Push a pointer to some values as a sample into the outlet.
	 *
	 * This is a lower-level function when data is available in some buffer.
	 * Handles type checking & conversion. The user needs to ensure that he/she has as many values
	 * as channels in the pointed-to location.
	 * @param data A pointer to values to push. The number of values pointed to must not be less
	 * than the number of channels in the sample.
	 * @param timestamp Optionally the capture time of the sample, in agreement with lsl_clock(); if
	 * omitted, the current time is assumed.
	 * @param pushthrough Whether to push the sample through to the receivers instead of buffering
	 * it into a chunk according to network speeds.
	 */
	void push_sample(const float *data, double timestamp = 0.0, bool pushthrough = true) {
		enqueue(data, timestamp, pushthrough);
	}
	void push_sample(const double *data, double timestamp = 0.0, bool pushthrough = true) {
		enqueue(data, timestamp, pushthrough);
	}
	void push_sample(const int64_t *data, double timestamp = 0.0, bool pushthrough = true) {
		enqueue(data, timestamp, pushthrough);
	}
	void push_sample(const int32_t *data, double timestamp = 0.0, bool pushthrough = true) {
		enqueue(data, timestamp, pushthrough);
	}
	void push_sample(const int16_t *data, double timestamp = 0.0, bool pushthrough = true) {
		enqueue(data, timestamp, pushthrough);
	}
	void push_sample(const char *data, double timestamp = 0.0, bool pushthrough = true) {
		enqueue(data, timestamp, pushthrough);
	}
	void push_sample(const std::string *data, double timestamp = 0.0, bool pushthrough = true) {
		enqueue(data, timestamp, pushthrough);
	}


	template <typename T>
	inline lsl_error_code_t push_sample_noexcept(
		const T *data, double timestamp = 0.0, bool pushthrough = true) noexcept {
		try {
			enqueue(data, timestamp, pushthrough);
			return lsl_no_error;
		} catch (std::range_error &e) {
			LOG_F(WARNING, "Error during push_sample: %s", e.what());
			return lsl_argument_error;
		} catch (std::invalid_argument &e) {
			LOG_F(WARNING, "Error during push_sample: %s", e.what());
			return lsl_argument_error;
		} catch (std::exception &e) {
			LOG_F(WARNING, "Unexpected error during push_sample: %s", e.what());
			return lsl_internal_error;
		}
	}

	/**
	 * Push a pointer to raw numeric data as one sample into the outlet.
	 * This is the lowest-level function; does no checking of any kind. Do not use for
	 * variable-size/string data-formatted channels.
	 * @param sample A pointer to the raw sample data to push.
	 * @param timestamp Optionally the capture time of the sample, in agreement with lsl_clock(); if
	 * omitted, the current time is assumed.
	 * @param pushthrough Whether to push the sample through to the receivers instead of buffering
	 * it into a chunk according to network speeds.
	 */
	void push_numeric_raw(const void *data, double timestamp = 0.0, bool pushthrough = true);

	//
	// === Pushing an chunk of samples into the outlet ===
	//

	/**
	 * Push a chunk of multiplexed samples into the send buffer.
	 *
	 * One timestamp per sample is provided.
	 *
	 * @warning The provided buffer size is measured in channel values (e.g. floats) rather than in
	 * samples.
	 * @param data_buffer A buffer of channel values holding the data for zero or more successive
	 * samples to send.
	 * @param timestamp_buffer A buffer of timestamp values holding time stamps for each sample in
	 * the data buffer.
	 * @param data_buffer_elements The number of data values (of type T) in the data buffer. Must be
	 * a multiple of the channel count.
	 * @param pushthrough Whether to push the chunk through to the receivers instead of buffering it
	 * with subsequent samples. Note that the chunk_size, if specified at outlet construction, takes
	 * precedence over the pushthrough flag.
	 */
	template <class T>
	void push_chunk_multiplexed(const T *data_buffer, const double *timestamp_buffer,
		std::size_t data_buffer_elements, bool pushthrough = true) {
		std::size_t num_chans = info().channel_count(),
					num_samples = data_buffer_elements / num_chans;
		if (data_buffer_elements % num_chans != 0)
			throw std::runtime_error("The number of buffer elements to send is not a multiple of "
									 "the stream's channel count.");
		if (!data_buffer) throw std::runtime_error("The data buffer pointer must not be NULL.");
		if (!timestamp_buffer)
			throw std::runtime_error("The timestamp buffer pointer must not be NULL.");
		for (std::size_t k = 0; k < num_samples; k++)
			enqueue(&data_buffer[k * num_chans], timestamp_buffer[k],
				pushthrough && k == num_samples - 1);
	}

	template <class T>
	int32_t push_chunk_multiplexed_noexcept(const T *data_buffer, const double *timestamp_buffer,
		std::size_t data_buffer_elements, bool pushthrough = true) noexcept {
		try {
			push_chunk_multiplexed(
				data_buffer, timestamp_buffer, data_buffer_elements, pushthrough);
			return lsl_no_error;
		} catch (std::range_error &e) {
			LOG_F(WARNING, "Error during push_chunk: %s", e.what());
			return lsl_argument_error;
		} catch (std::invalid_argument &e) {
			LOG_F(WARNING, "Error during push_chunk: %s", e.what());
			return lsl_argument_error;
		} catch (std::exception &e) {
			LOG_F(WARNING, "Unexpected error during push_chunk: %s", e.what());
			return lsl_internal_error;
		}
	}

	/**
	 * Push a chunk of multiplexed samples into the send buffer. Single timestamp provided.
	 *
	 * @warning The provided buffer size is measured in channel values (e.g., floats) rather than in
	 * samples.
	 * @param buffer A buffer of channel values holding the data for zero or more successive samples
	 * to send.
	 * @param buffer_elements The number of channel values (of type T) in the buffer. Must be a
	 * multiple of the channel count.
	 * @param timestamp Optionally the capture time of the most recent sample, in agreement with
	 * lsl_clock(); if omitted, the current time is used. The time stamps of other samples are
	 * automatically derived based on the sampling rate of the stream.
	 * @param pushthrough Whether to push the chunk through to the receivers instead of buffering it
	 * with subsequent samples. Note that the chunk_size, if specified at outlet construction, takes
	 * precedence over the pushthrough flag.
	 */
	template <class T>
	void push_chunk_multiplexed(const T *buffer, std::size_t buffer_elements,
		double timestamp = 0.0, bool pushthrough = true) {
		std::size_t num_chans = info().channel_count(), num_samples = buffer_elements / num_chans;
		if (buffer_elements % num_chans != 0)
			throw std::runtime_error("The number of buffer elements to send is not a multiple of "
									 "the stream's channel count.");
		if (!buffer)
			throw std::runtime_error("The number of buffer elements to send is not a multiple of "
									 "the stream's channel count.");
		if (num_samples > 0) {
			if (timestamp == 0.0) timestamp = lsl_clock();
			if (info().nominal_srate() != IRREGULAR_RATE)
				timestamp = timestamp - (num_samples - 1) / info().nominal_srate();
			push_sample(buffer, timestamp, pushthrough && (num_samples == 1));
			for (std::size_t k = 1; k < num_samples; k++)
				push_sample(&buffer[k * num_chans], DEDUCED_TIMESTAMP,
					pushthrough && (k == num_samples - 1));
		}
	}

	template <class T>
	int32_t push_chunk_multiplexed_noexcept(const T *data, std::size_t data_elements,
		double timestamp = 0.0, bool pushthrough = true) noexcept {
		try {
			push_chunk_multiplexed(data, data_elements, timestamp, pushthrough);
			return lsl_no_error;
		} catch (std::range_error &e) {
			LOG_F(WARNING, "Error during push_chunk: %s", e.what());
			return lsl_argument_error;
		} catch (std::invalid_argument &e) {
			LOG_F(WARNING, "Error during push_chunk: %s", e.what());
			return lsl_argument_error;
		} catch (std::exception &e) {
			LOG_F(WARNING, "Unexpected error during push_chunk: %s", e.what());
			return lsl_internal_error;
		}
	}

	// === Misc Features ===

	/**
	 * Retrieve the stream info associated with this outlet.
	 *
	 * This is the constant meta-data header that was used to create the stream.
	 */
	const stream_info_impl &info() const { return *info_; }

	/// Check whether consumers are currently registered.
	bool have_consumers();

	/// Wait until some consumer shows up.
	bool wait_for_consumers(double timeout = FOREVER);

private:
	/// Instantiate a new server stack.
	void instantiate_stack(udp udp_protocol);

	/// Allocate and enqueue a new sample into the send buffer.
	template <class T> void enqueue(const T *data, double timestamp, bool pushthrough);

	/**
	 * Check whether some given number of channels matches the stream's channel_count.
	 * Throws an error if not.
	 */
	void check_numchan(uint32_t chns) {
		if (chns != info_->channel_count())
			throw std::range_error("The provided sample data has a different length than the "
								   "stream's number of channels.");
	}

	/// a factory for samples of appropriate type
	factory_p sample_factory_;
	/// the preferred chunk size
	int32_t chunk_size_;
	/// stream_info shared between the various server instances
	stream_info_impl_p info_;
	/// the single-producer, multiple-receiver send buffer
	send_buffer_p send_buffer_;
	/// the IO service objects
	io_context_p io_ctx_data_, io_ctx_service_;

	/// the threaded TCP data server
	tcp_server_p tcp_server_;
	/// the UDP timing & ident service(s); two if using both IP stacks
	std::vector<udp_server_p> udp_servers_;
	/// UDP multicast responders for service discovery (time features disabled);
	/// also using only the allowed IP stacks
	std::vector<udp_server_p> responders_;
	/// threads that handle the I/O operations (two per stack: one for UDP and one for TCP)
	std::vector<thread_p> io_threads_;
};

} // namespace lsl

#endif
