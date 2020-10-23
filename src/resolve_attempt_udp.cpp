#include "resolve_attempt_udp.h"
#include "api_config.h"
#include "resolver_impl.h"
#include "socket_utils.h"
#include <boost/asio/ip/multicast.hpp>
#include <sstream>

using namespace lsl;
using namespace lslboost::asio;
using err_t = const lslboost::system::error_code &;

resolve_attempt_udp::resolve_attempt_udp(io_context &io, const udp &protocol,
	const std::vector<udp::endpoint> &targets, const std::string &query, result_container &results,
	std::mutex &results_mut, double cancel_after, cancellable_registry *registry)
	: io_(io), results_(results), results_mut_(results_mut), cancel_after_(cancel_after),
	  cancelled_(false), targets_(targets), query_(query), unicast_socket_(io),
	  broadcast_socket_(io), multicast_socket_(io), recv_socket_(io), cancel_timer_(io) {
	// open the sockets that we might need
	recv_socket_.open(protocol);
	try {
		bind_port_in_range(recv_socket_, protocol);
	} catch (std::exception &e) {
		LOG_F(WARNING,
			"Could not bind to a port in the configured port range; using a randomly assigned one: "
			"%s",
			e.what());
	}
	unicast_socket_.open(protocol);
	try {
		broadcast_socket_.open(protocol);
		broadcast_socket_.set_option(socket_base::broadcast(true));
	} catch (std::exception &e) {
		LOG_F(WARNING, "Cannot open UDP broadcast socket for resolves: %s", e.what());
	}
	try {
		multicast_socket_.open(protocol);
		multicast_socket_.set_option(
			ip::multicast::hops(api_config::get_instance()->multicast_ttl()));
	} catch (std::exception &e) {
		LOG_F(WARNING, "Cannot open UDP multicast socket for resolves: %s", e.what());
	}

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
	if (registry) register_at(registry);
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
	send_next_query(targets_.begin());

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
	recv_socket_.async_receive_from(buffer(resultbuf_), remote_endpoint_,
		[shared_this = shared_from_this()](
			err_t err, size_t len) { shared_this->handle_receive_outcome(err, len); });
}

void resolve_attempt_udp::handle_receive_outcome(error_code err, std::size_t len) {
	if (cancelled_ || err == error::operation_aborted || err == error::not_connected ||
		err == error::not_socket)
		return;

	if (!err) {
		try {
			// first parse & check the query id
			std::istringstream is(std::string(resultbuf_, len));
			std::string returned_id;
			getline(is, returned_id);
			returned_id = trim(returned_id);
			if (returned_id == query_id_) {
				// parse the rest of the query into a stream_info
				stream_info_impl info;
				std::ostringstream os;
				os << is.rdbuf();
				info.from_shortinfo_message(os.str());
				std::string uid = info.uid();
				{
					// update the results
					std::lock_guard<std::mutex> lock(results_mut_);
					if (results_.find(uid) == results_.end())
						results_[uid] = std::make_pair(info, lsl_clock()); // insert new result
					else
						results_[uid].second = lsl_clock(); // update only the receive time
					// ... also update the address associated with the result (but don't
					// override the address of an earlier record for this stream since this
					// would be the faster route)
					if (remote_endpoint_.address().is_v4()) {
						if (results_[uid].first.v4address().empty())
							results_[uid].first.v4address(remote_endpoint_.address().to_string());
					} else {
						if (results_[uid].first.v6address().empty())
							results_[uid].first.v6address(remote_endpoint_.address().to_string());
					}
				}
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

void resolve_attempt_udp::send_next_query(endpoint_list::const_iterator next) {
	if (next == targets_.end() || cancelled_) return;

	udp::endpoint ep(*next++);
	// endpoint matches our active protocol?
	if (ep.protocol() == recv_socket_.local_endpoint().protocol()) {
		// select socket to use
		udp::socket &sock =
			(ep.address() == ip::address_v4::broadcast())
				? broadcast_socket_
				: (ep.address().is_multicast() ? multicast_socket_ : unicast_socket_);
		// and send the query over it
		sock.async_send_to(lslboost::asio::buffer(query_msg_), ep,
			[shared_this = shared_from_this(), next](err_t err, size_t) {
				if (!shared_this->cancelled_ && err != error::operation_aborted &&
					err != error::not_connected && err != error::not_socket)
					shared_this->send_next_query(next);
			});
	} else
		// otherwise just go directly to the next query
		send_next_query(next);
}

void resolve_attempt_udp::do_cancel() {
	try {
		cancelled_ = true;
		if (unicast_socket_.is_open()) unicast_socket_.close();
		if (multicast_socket_.is_open()) multicast_socket_.close();
		if (broadcast_socket_.is_open()) broadcast_socket_.close();
		if (recv_socket_.is_open()) recv_socket_.close();
		cancel_timer_.cancel();
	} catch (std::exception &e) {
		LOG_F(WARNING,
			"Unexpected error while trying to cancel operations of resolve_attempt_udp: %s",
			e.what());
	}
}
