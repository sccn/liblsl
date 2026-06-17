#include "resolve_attempt_tcp.h"
#include "common.h"
#include "resolver_impl.h"
#include "stream_info_impl.h"
#include <algorithm>
#include <asio/buffer.hpp>
#include <asio/connect.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>
#include <exception>
#include <loguru.hpp>
#include <mutex>
#include <utility>

using namespace lsl;
using err_t = const asio::error_code &;

/// Maximum number of TCP probes in flight at once (one socket each).
static const std::size_t MAX_CONCURRENT_PROBES = 16;

resolve_attempt_tcp::resolve_attempt_tcp(asio::io_context &io,
	const std::vector<tcp::endpoint> &targets, const std::string &query, resolver_impl &resolver,
	double cancel_after)
	: io_(io), resolver_(resolver), cancel_after_(cancel_after), cancelled_(false),
	  targets_(targets), next_target_(0), active_workers_(0), cancel_timer_(io) {
	// Precompute the query message. Unlike the UDP query there is no return port or query id:
	// the reply comes back on this very connection and the TCP shortinfo responder replies with
	// the bare shortinfo message (no query-id prefix).
	query_msg_ = "LSL:shortinfo\r\n" + query + "\r\n";
	// register ourselves as a candidate for cancellation
	register_at(&resolver);
}

resolve_attempt_tcp::~resolve_attempt_tcp() { unregister_from_all(); }

void resolve_attempt_tcp::begin() {
	// arm the cancel timer
	if (cancel_after_ != FOREVER) {
		cancel_timer_.expires_after(timeout_sec(cancel_after_));
		cancel_timer_.async_wait([shared_this = shared_from_this(), this](err_t err) {
			if (!err) do_cancel();
		});
	}
	// launch the worker pool
	active_workers_ = std::min(MAX_CONCURRENT_PROBES, targets_.size());
	worker_sockets_.resize(active_workers_);
	std::size_t workers = active_workers_;
	for (std::size_t w = 0; w < workers; w++) probe_next(w);
}

void resolve_attempt_tcp::cancel() {
	post(io_, [shared_this = shared_from_this()]() { shared_this->do_cancel(); });
}

void resolve_attempt_tcp::probe_next(std::size_t worker) {
	if (cancelled_) return;
	if (next_target_ >= targets_.size()) {
		// this worker is out of endpoints; once all workers are done, drop the cancel timer so
		// the attempt can finish (and the resolver can schedule a fresh burst)
		if (--active_workers_ == 0) cancel_timer_.cancel();
		return;
	}
	const tcp::endpoint ep = targets_[next_target_++];
	auto sock = std::make_shared<tcp_socket>(io_);
	worker_sockets_[worker] = sock;

	auto self = shared_from_this();
	sock->async_connect(ep, [self, this, worker, ep, sock](err_t err) {
		if (cancelled_) return;
		if (err) {
			// closed / filtered / unreachable port: move on to the next target
			probe_next(worker);
			return;
		}
		// connected: send the shortinfo query
		asio::async_write(*sock, asio::buffer(query_msg_),
			[self, this, worker, ep, sock](err_t err, std::size_t /*unused*/) {
				if (cancelled_) return;
				if (err) {
					probe_next(worker);
					return;
				}
				// read the reply until the outlet closes the connection (EOF)
				auto reply = std::make_shared<std::string>();
				asio::async_read(*sock, asio::dynamic_buffer(*reply),
					[self, this, worker, ep, sock, reply](err_t err, std::size_t /*unused*/) {
						if (cancelled_) return;
						if (!err || err == asio::error::eof) handle_reply(*reply, ep);
						probe_next(worker);
					});
			});
	});
}

void resolve_attempt_tcp::handle_reply(const std::string &reply, const tcp::endpoint &ep) {
	if (reply.empty()) return; // no match: the responder closed without replying
	try {
		stream_info_impl info;
		info.from_shortinfo_message(reply);
		// The shortinfo XML carries the ports but not the host address; fill it in from the
		// endpoint we connected to (mirrors the UDP resolver using the reply's source address).
		const std::string addr = ep.address().to_string();
		std::string uid = info.uid();
		{
			std::lock_guard<std::mutex> lock(resolver_.results_mut_);
			auto it = resolver_.results_.find(uid);
			if (it == resolver_.results_.end())
				it = resolver_.results_.emplace(uid, std::make_pair(info, lsl_clock())).first;
			else
				it->second.second = lsl_clock();
			auto &stored_info = it->second.first;
			if (ep.address().is_v4()) {
				if (stored_info.v4address().empty()) stored_info.v4address(addr);
			} else {
				if (stored_info.v6address().empty()) stored_info.v6address(addr);
			}
		}
		// prepone the next cancellation check so a hit cancels the remaining (slow) probes
		if (resolver_.check_cancellation_criteria()) resolver_.cancel_ongoing_resolve();
	} catch (std::exception &e) {
		LOG_F(WARNING, "resolve_attempt_tcp: could not parse a reply from %s: %s",
			ep.address().to_string().c_str(), e.what());
	}
}

void resolve_attempt_tcp::do_cancel() {
	try {
		cancelled_ = true;
		for (auto &sock : worker_sockets_)
			if (sock && sock->is_open()) sock->close();
		cancel_timer_.cancel();
	} catch (std::exception &e) {
		LOG_F(WARNING, "Unexpected error while trying to cancel a resolve_attempt_tcp: %s",
			e.what());
	}
}
