#include "resolve_attempt_udp.h"
#include "api_config.h"
#include "netinterfaces.h"
#include "resolver_impl.h"
#include "socket_utils.h"
#include "util/strfuns.hpp"
#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/multicast.hpp>
#include <exception>
#include <loguru.hpp>
#include <sstream>

using namespace lsl;
using err_t = const asio::error_code &;
using asio::ip::multicast::outbound_interface;

resolve_attempt_udp::resolve_attempt_udp(asio::io_context &io, const udp &protocol,
	const std::vector<udp::endpoint> &targets, const std::string &query, resolver_impl &resolver,
	double cancel_after)
	: io_(io), resolver_(resolver), cancel_after_(cancel_after), cancelled_(false),
	  targets_(targets), query_(query),
	  multicast_interfaces(api_config::get_instance()->multicast_interfaces), recv_socket_(io),
	  cancel_timer_(io) {
	// Open the single socket used for BOTH sending queries and receiving replies, so that the
	// query's source port matches the return port we advertise in it (see header / 1a).
	recv_socket_.open(protocol);
	try {
		bind_port_in_range(recv_socket_, protocol);
	} catch (std::exception &e) {
		LOG_F(WARNING,
			"Could not bind to a port in the configured port range; using a randomly assigned one: "
			"%s",
			e.what());
	}
	// The receive socket must also be able to send to broadcast addresses and to carry the
	// multicast TTL, since it now sends every flavor of query (unicast / broadcast / multicast).
	asio::error_code ec;
	recv_socket_.set_option(asio::socket_base::broadcast(true), ec);
	if (ec) LOG_F(WARNING, "Cannot enable broadcast on the resolve socket: %s", ec.message().c_str());
	recv_socket_.set_option(
		asio::ip::multicast::hops(api_config::get_instance()->multicast_ttl()), ec);
	if (ec) LOG_F(WARNING, "Cannot set the multicast TTL on the resolve socket: %s", ec.message().c_str());

	// precalc the query id (hash of the query string, as string)
	query_id_ = std::to_string(std::hash<std::string>()(query));
	// precalc the query message
	std::ostringstream os;
	os.precision(16);
	os << "LSL:shortinfo\r\n";
	os << query_ << "\r\n";
	os << recv_socket_.local_endpoint().port() << " " << query_id_ << "\r\n";
	query_msg_ = os.str();

	DLOG_F(2, "Waiting for query results (port %d) for %s", recv_socket_.local_endpoint().port(),
		query_msg_.c_str());

	// register ourselves as a candidate for cancellation
	register_at(&resolver);
}

resolve_attempt_udp::~resolve_attempt_udp() {
	// make sure that the cancel is unregistered before the resolve attempt is being deleted...
	unregister_from_all();
}

// === externally-triggered asynchronous commands ===

void resolve_attempt_udp::begin() {
	// initiate the result gathering chain
	receive_next_result();
	// initiate the send chain
	send_next_query(targets_.begin(), multicast_interfaces.begin());

	// also initiate the cancel event, if desired
	if (cancel_after_ != FOREVER) {
		cancel_timer_.expires_after(timeout_sec(cancel_after_));
		cancel_timer_.async_wait([shared_this = shared_from_this(), this](err_t err) {
			if (!err) do_cancel();
		});
	}
}

void resolve_attempt_udp::cancel() {
	post(io_, [shared_this = shared_from_this()]() { shared_this->do_cancel(); });
}


// === receive loop ===

void resolve_attempt_udp::receive_next_result() {
	recv_socket_.async_receive_from(asio::buffer(resultbuf_), remote_endpoint_,
		[shared_this = shared_from_this()](
			err_t err, size_t len) { shared_this->handle_receive_outcome(err, len); });
}

void resolve_attempt_udp::handle_receive_outcome(err_t err, std::size_t len) {
	if (cancelled_ || err == asio::error::operation_aborted || err == asio::error::not_connected ||
		err == asio::error::not_socket)
		return;

	if (!err) {
		try {
			// first parse & check the query id
			char *bufend = resultbuf_ + len;
			char *newlinepos = resultbuf_;
			// find the end of the line
			while (newlinepos != bufend && *newlinepos != '\n') ++newlinepos;
			std::string returned_id(resultbuf_, trim_end(resultbuf_, newlinepos));

			if (returned_id == query_id_ && newlinepos != bufend) {
				// parse the rest of the query into a stream_info
				stream_info_impl info;
				info.from_shortinfo_message(std::string(newlinepos, bufend));
				std::string uid = info.uid();
				{
					// update the results
					std::lock_guard<std::mutex> lock(resolver_.results_mut_);
					auto it = resolver_.results_.find(uid);
					if (it == resolver_.results_.end())
						// insert new result, store iterator in it
						it = resolver_.results_.emplace(uid, std::make_pair(info, lsl_clock()))
								 .first;
					else
						it->second.second = lsl_clock(); // update only the receive time
					auto &stored_info = it->second.first;
					// ... also update the address associated with the result (but don't
					// override the address of an earlier record for this stream since this
					// would be the faster route)
					if (remote_endpoint_.address().is_v4()) {
						if (stored_info.v4address().empty())
							stored_info.v4address(remote_endpoint_.address().to_string());
					} else {
						if (stored_info.v6address().empty())
							stored_info.v6address(remote_endpoint_.address().to_string());
					}
				}
				// prepone the next cancellation check, i.e. when all needed streams are found,
				// cancel immediately rather than when a wave timer is due half a second later
				if (resolver_.check_cancellation_criteria())
					resolver_.cancel_ongoing_resolve();
			}
		} catch (std::exception &e) {
			LOG_F(WARNING, "resolve_attempt_udp: hiccup while processing the received data: %s",
				e.what());
		}
	}
	// ask for the next result
	receive_next_result();
}


// === send loop ===

void resolve_attempt_udp::send_next_query(
	endpoint_list::const_iterator next, mcast_interface_list::const_iterator mcit) {
	if (cancelled_ || mcit == multicast_interfaces.end()) return;
	auto proto = recv_socket_.local_endpoint().protocol();
	if (next == targets_.begin()) {
		// Mismatching protocols? Skip this round
		if (mcit->addr.is_v4() != (proto == asio::ip::udp::v4()))
			next = targets_.end();
		else
			recv_socket_.set_option(mcit->addr.is_v4() ? outbound_interface(mcit->addr.to_v4())
													   : outbound_interface(mcit->ifindex));
	}
	if (next != targets_.end()) {
		udp::endpoint ep(*next++);
		// endpoint matches our active protocol?
		if (ep.protocol() == recv_socket_.local_endpoint().protocol()) {
			// Send every query (unicast / broadcast / multicast) from the receive socket so the
			// datagram source port equals the advertised return port (firewall-friendly; 1a).
			auto keepalive(shared_from_this());
			recv_socket_.async_send_to(asio::buffer(query_msg_), ep,
				[shared_this = shared_from_this(), next, mcit](err_t err, size_t /*unused*/) {
					if (!shared_this->cancelled_ && err != asio::error::operation_aborted &&
						err != asio::error::not_connected && err != asio::error::not_socket)
						shared_this->send_next_query(next, mcit);
				});
		} else
			// otherwise just go directly to the next query
			send_next_query(next, mcit);
	} else
		// Restart from the next interface
		send_next_query(targets_.begin(), ++mcit);
}

void resolve_attempt_udp::do_cancel() {
	try {
		cancelled_ = true;
		if (recv_socket_.is_open()) recv_socket_.close();
		cancel_timer_.cancel();
	} catch (std::exception &e) {
		LOG_F(WARNING,
			"Unexpected error while trying to cancel operations of resolve_attempt_udp: %s",
			e.what());
	}
}
