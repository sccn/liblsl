#ifndef RESOLVE_ATTEMPT_TCP_H
#define RESOLVE_ATTEMPT_TCP_H

#include "cancellation.h"
#include "socket_utils.h"
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/steady_timer.hpp>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

using asio::ip::tcp;
using err_t = const asio::error_code &;

namespace lsl {
class resolver_impl;

using steady_timer = asio::basic_waitable_timer<asio::chrono::steady_clock,
	asio::wait_traits<asio::chrono::steady_clock>, asio::io_context::executor_type>;

/**
 * An asynchronous resolve attempt that probes a set of TCP endpoints directly.
 *
 * For each endpoint it opens a TCP connection, sends an `LSL:shortinfo` query, and parses the
 * outlet's reply into a stream_info that is stored in the shared resolver results. Unlike the UDP
 * resolver this requires one connection per endpoint, so probes run through a small pool of
 * concurrent workers. Used as a firewall-robust (but slow) discovery fallback, gated behind the
 * `lab.ResolveOverTCP` config option.
 */
class resolve_attempt_tcp final : public cancellable_obj,
								  public std::enable_shared_from_this<resolve_attempt_tcp> {
public:
	/**
	 * Instantiate and set up a new TCP resolve attempt.
	 *
	 * @param io The io_context that will run the async operations.
	 * @param targets The TCP endpoints to probe (host x port-range).
	 * @param query The query string the outlet must match to reply.
	 * @param resolver The resolver whose results container is populated.
	 * @param cancel_after Time after which the attempt is automatically cancelled.
	 */
	resolve_attempt_tcp(asio::io_context &io, const std::vector<tcp::endpoint> &targets,
		const std::string &query, resolver_impl &resolver, double cancel_after = 5.0);

	/// Destructor.
	~resolve_attempt_tcp() final;

	/// Start probing asynchronously.
	void begin();

	/// Cancel operations asynchronously and destructively.
	void cancel() override;

private:
	/// Probe the next not-yet-taken target endpoint using the given worker's socket slot.
	void probe_next(std::size_t worker);

	/// Parse a reply received from a given endpoint into the resolver results.
	void handle_reply(const std::string &reply, const tcp::endpoint &ep);

	/// Cancel the outstanding operations.
	void do_cancel();

	/// the IO service that executes our actions
	asio::io_context &io_;
	/// the resolver associated with this attempt
	resolver_impl &resolver_;
	/// the timeout for giving up
	double cancel_after_;
	/// whether the operation has been cancelled
	bool cancelled_;
	/// the endpoints to probe
	std::vector<tcp::endpoint> targets_;
	/// the message we send ("LSL:shortinfo\r\n<query>\r\n")
	std::string query_msg_;
	/// index of the next target to hand to a worker
	std::size_t next_target_;
	/// number of workers that still have endpoints to probe
	std::size_t active_workers_;
	/// per-worker current socket (kept so a cancel can close in-flight connects)
	std::vector<std::shared_ptr<tcp_socket>> worker_sockets_;
	/// timer to schedule the cancel action
	steady_timer cancel_timer_;
};
} // namespace lsl

#endif
