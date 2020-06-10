#ifndef RESOLVE_ATTEMPT_UDP_H
#define RESOLVE_ATTEMPT_UDP_H

#include "cancellation.h"
#include "forward.h"
#include "netinterfaces.h"
#include "stream_info_impl.h"
#include "socket_utils.h"
#include <asio/ip/multicast.hpp>
#include <asio/steady_timer.hpp>
#include <map>
#include <memory>
#include <string>
#include <vector>

using asio::ip::udp;
using err_t = const asio::error_code &;

namespace lsl {

using steady_timer = asio::basic_waitable_timer<asio::chrono::steady_clock, asio::wait_traits<asio::chrono::steady_clock>, asio::io_context::executor_type>;

/// A container for resolve results (map from stream instance UID onto (stream_info,receive-time)).
typedef std::map<std::string, std::pair<stream_info_impl, double>> result_container;
/// A container for outgoing multicast interfaces
typedef std::vector<class netif> mcast_interface_list;

/**
 * An asynchronous resolve attempt for a single query targeted at a set of endpoints, via UDP.
 *
 * A resolve attempt is an asynchronous operation submitted to an IO object, which amounts to a
 * sequence of query packet sends (one for each endpoint in the list) and a sequence of result
 * packet receives. The operation will wait for return packets until either a particular timeout has
 * been reached or until it is cancelled via the cancel() method.
 */
class resolve_attempt_udp final : public cancellable_obj,
								  public std::enable_shared_from_this<resolve_attempt_udp> {
	using endpoint_list = std::vector<udp::endpoint>;

public:
	/**
	 * Instantiate and set up a new resolve attempt.
	 *
	 * @param io The io_context that will run the async operations.
	 * @param protocol The protocol (either udp::v4() or udp::v6()) to use for communications;
	 * only the subset of target addresses matching this protocol will be considered.
	 * @param targets A list of udp::endpoint that should be targeted by this query.
	 * @param query The query string to send (usually a set of conditions on the properties of the
	 * stream info that should be searched, for example `name='BioSemi' and type='EEG'`.
	 * See lsl_stream_info_matches_query for the definition of a query.
	 * @param results Reference to a container into which results are stored; potentially shared
	 * with other parallel resolve operations. Since this is not thread-safe all operations
	 * modifying this must run on the same single-threaded IO service.
	 * @param results_mut Reference to a mutex that protects the container.
	 * @param cancel_after The time duration after which the attempt is automatically cancelled,
	 * i.e. the receives are ended.
	 * @param registry A registry where the attempt can register itself as active so it can be
	 * cancelled during shutdown.
	 */
	resolve_attempt_udp(asio::io_context &io, const udp &protocol,
		const std::vector<udp::endpoint> &targets, const std::string &query,
		resolver_impl &resolver, double cancel_after = 5.0);

	/// Destructor
	~resolve_attempt_udp() final;

	/// Start the attempt asynchronously.
	void begin();

	/// Cancel operations asynchronously, and destructively.
	/// Note that this mostly serves to expedite the destruction of the object,
	/// which would happen anyway after some time.
	/// As the attempt instance is owned by the handler chains
	/// the cancellation eventually leads to the destruction of the object.
	void cancel() override;

private:
	// === send and receive handlers ===

	/// This function asks to receive the next result packet.
	void receive_next_result();

	/// Thos function starts an async send operation for the given current endpoint.
	void send_next_query(
		endpoint_list::const_iterator next, mcast_interface_list::const_iterator mcit);

	/// Handler that gets called when a receive has completed.
	void handle_receive_outcome(err_t err, std::size_t len);

	// === cancellation ===

	/// Cancel the outstanding operations.
	void do_cancel();


	// data shared with the resolver_impl
	/// reference to the IO service that executes our actions
	asio::io_context &io_;
	/// the resolver associated with this attempt
	resolver_impl &resolver_;

	// constant over the lifetime of this attempt
	/// the timeout for giving up
	double cancel_after_;
	/// whether the operation has been cancelled
	bool cancelled_;
	/// list of endpoints that should receive the query
	std::vector<udp::endpoint> targets_;
	/// the query string
	std::string query_;
	/// the query message that we're sending
	std::string query_msg_;
	/// the (more or less) unique id for this query
	std::string query_id_;

	// data maintained/modified across handler invocations
	/// the endpoint from which we received the last result
	udp::endpoint remote_endpoint_;
	/// holds a single result received from the net
	char resultbuf_[65536];

	// IO objects
	/// socket to send data over (for unicasts)
	udp_socket unicast_socket_;
	/// socket to send data over (for broadcasts)
	udp_socket broadcast_socket_;
	/// socket to send data over (for multicasts)
	udp_socket multicast_socket_;
	/// Interface addresses to send multicast packets from
	const mcast_interface_list &multicast_interfaces;
	/// socket to receive replies (always unicast)
	udp_socket recv_socket_;
	/// timer to schedule the cancel action
	steady_timer cancel_timer_;
};
} // namespace lsl

#endif
