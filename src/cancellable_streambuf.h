#pragma once
//
// cancellable_streambuf.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// This is a carbon copy of basic_socket_streambuf.hpp, adding a cancel() member function
// (an removing support for unbuffered I/O).
//
// Copyright (c) 2003-2011 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Modified by Christian A. Kothe, 2012
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#define BOOST_ASIO_NO_DEPRECATED
#include "cancellation.h"
#include <asio/basic_stream_socket.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <exception>
#include <streambuf>

using asio::io_context;

namespace lsl {
using Protocol = asio::ip::tcp;
using Socket = asio::basic_stream_socket<Protocol, asio::io_context::executor_type>;
/// Iostream streambuf for a socket.
class cancellable_streambuf final : public std::streambuf,
									private asio::io_context,
									private Socket,
									public lsl::cancellable_obj {
public:
	/// Construct a cancellable_streambuf without establishing a connection.
	cancellable_streambuf() : io_context(1), Socket(as_context()) { init_buffers(); }

	/// Destructor flushes buffered data.
	~cancellable_streambuf() override {
		// no cancel() can fire after this call
		unregister_from_all();
		if (pptr() != pbase()) overflow(traits_type::eof());
	}

	/**
	 * Cancel the current stream operations destructively.
	 * All blocking operations will fail after a cancel() has been issued,
	 * and the stream buffer cannot be reused.
	 */
	void cancel() override {
		cancel_issued_ = true;
		std::lock_guard<std::recursive_mutex> lock(cancel_mut_);
		cancel_started_ = false;
		asio::post(this->as_context(), [this]() { close_if_open(); });
	}


	/// Establish a connection.
	/**
	 * This function establishes a connection to the specified endpoint.
	 *
	 * @return \c this if a connection was successfully established, a null
	 * pointer otherwise.
	 */
	cancellable_streambuf *connect(const Protocol::endpoint &endpoint) {
		{
			std::lock_guard<std::recursive_mutex> lock(cancel_mut_);
			if (cancel_issued_)
				throw std::runtime_error(
					"Attempt to connect() a cancellable_streambuf after it has been cancelled.");

			init_buffers();
			socket().close(ec_);
			socket().async_connect(
				endpoint, [this](const asio::error_code &ec) { this->ec_ = ec; });
			this->as_context().restart();
		}
		ec_ = asio::error::would_block;
		do as_context().run_one();
		while (!cancel_issued_ && ec_ == asio::error::would_block);
		return !ec_ ? this : nullptr;
	}

	/// Close the connection.
	/**
	 * @return \c this if a connection was successfully established, a null
	 * pointer otherwise.
	 */
	cancellable_streambuf *close() {
		sync();
		socket().close(ec_);
		if (!ec_) init_buffers();
		return !ec_ ? this : nullptr;
	}

	/** Get the last error associated with the stream buffer.
	 * @return An \c error_code corresponding to the last error from the stream
	 * buffer.
	 */
	const asio::error_code &error() const { return ec_; }

protected:
	/// Close the socket if it's open.
	void close_if_open() {
		if (!cancel_started_ && socket().is_open()) {
			cancel_started_ = true;
			socket().close();
		}
	}

	/// Convenience method to call methods inherited from `Socket`
	Socket &socket() { return *this; }

	/// Convenience method to call methods inherited from `io_context`
	asio::io_context &as_context() { return *this; }

	/// Make sure that a cancellation, if issued, is not being eaten by `io_context::reset()`
	void protected_reset() {
		std::lock_guard<std::recursive_mutex> lock(cancel_mut_);
		// if the cancel() comes between completion of a run_one() and this call, close will be
		// issued right here at the next opportunity
		if (cancel_issued_) close_if_open();
		this->as_context().restart();
		// if the cancel() comes between this call and a completion of run_one(), the posted close
		// will be processed by the run_one
	}

	int_type underflow() override {
		if (gptr() == egptr()) {
			std::size_t bytes_transferred_;
			socket().async_receive(asio::buffer(asio::buffer(get_buffer_) + putback_max),
				[this, &bytes_transferred_](
					const asio::error_code &ec, std::size_t bytes_transferred = 0) {
					this->ec_ = ec;
					bytes_transferred_ = bytes_transferred;
				});

			ec_ = asio::error::would_block;
			protected_reset(); // line changed for lsl
			do as_context().run_one();
			while (!cancel_issued_ && ec_ == asio::error::would_block);
			if (ec_) return traits_type::eof();

			setg(&get_buffer_[0], &get_buffer_[0] + putback_max,
				&get_buffer_[0] + putback_max + bytes_transferred_);
			return traits_type::to_int_type(*gptr());
		} else
			return traits_type::eof();
	}

	int_type overflow(int_type c) override {
		// Send all data in the output buffer.
		asio::const_buffer buffer = asio::buffer(pbase(), pptr() - pbase());
		while (asio::buffer_size(buffer) > 0) {
			std::size_t bytes_transferred_;
			socket().async_send(
				asio::buffer(buffer), [this, &bytes_transferred_](const asio::error_code &ec,
										  std::size_t bytes_transferred) {
					this->ec_ = ec;
					bytes_transferred_ = bytes_transferred;
				});
			ec_ = asio::error::would_block;
			protected_reset(); // line changed for lsl
			do as_context().run_one();
			while (!cancel_issued_ && ec_ == asio::error::would_block);
			if (ec_) return traits_type::eof();
			buffer = buffer + bytes_transferred_;
		}
		setp(&put_buffer_[0], &put_buffer_[0] + sizeof(put_buffer_));

		// If the new character is eof then our work here is done.
		if (traits_type::eq_int_type(c, traits_type::eof())) return traits_type::not_eof(c);

		// Add the new character to the output buffer.
		*pptr() = traits_type::to_char_type(c);
		pbump(1);
		return c;
	}

	int sync() override { return overflow(traits_type::eof()); }

	std::streambuf *setbuf(char_type *, std::streamsize) override {
		// this feature was stripped out...
		return nullptr;
	}

	void init_buffers() {
		setg(&get_buffer_[0], &get_buffer_[0] + putback_max, &get_buffer_[0] + putback_max);
		setp(&put_buffer_[0], &put_buffer_[0] + sizeof(put_buffer_));
	}

	enum { putback_max = 8 };
	enum { buffer_size = 16384 };
	char get_buffer_[buffer_size], put_buffer_[buffer_size];
	asio::error_code ec_;
	std::atomic<bool> cancel_issued_{false};
	bool cancel_started_{false};
	std::recursive_mutex cancel_mut_;
};
} // namespace lsl
