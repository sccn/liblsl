#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "forward.h"
#include "socket_utils.h"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>

using asio::ip::tcp;
using err_t = const asio::error_code &;

namespace lsl {

/// shared pointer to a socket
using tcp_socket_p = std::shared_ptr<tcp_socket>;
/// shared pointer to an acceptor socket
using tcp_acceptor_p = std::unique_ptr<tcp_acceptor>;

/**
 * The TCP data server.
 *
 * Acts as a TCP server on a free port (in the configured port range), and understands the following
 * messages:
 *  - `LSL:streamfeed`: A request to receive streaming data on the connection. The server responds
 * with the shortinfo, two samples filled with a test pattern, followed by samples until the server
 * outlet goes out of existence.
 *  - `LSL:fullinfo`: A request for the stream_info served by this server.
 *  - `LSL:shortinfo`: A request for the stream_info served by this server if matching the provided
 * query string. The short version of the stream_info (empty `<desc>` element) is returned.
 */
class tcp_server : public std::enable_shared_from_this<tcp_server> {
public:
	/**
	 * Construct a new TCP server for a stream outlet.
	 *
	 * This opens a new TCP server port (in the allowed range) and, if successful, updates the
	 * stream_info object with the data of this connection. To have it serve connection requests,
	 * the member function begin_serving() must be called once. The latter should ideally not be
	 * done before the UDP service port has been successfully initialized, as well.
	 * @param info A stream_info that is shared with other server objects.
	 * @param io An io_context that is shared with other server objects.
	 * @param sendbuf A send buffer that is shared with other server objects.
	 * @param factory A sample factory that is shared with other server objects.
	 * @param protocol The protocol (IPv4 or IPv6) that shall be serviced by this server.
	 * @param chunk_size The preferred chunk size, in samples. If 0, the pushthrough flag determines
	 * the effective chunking.
	 */
	tcp_server(stream_info_impl_p info, io_context_p io, send_buffer_p sendbuf, factory_p factory,
		int chunk_size, bool allow_v4, bool allow_v6);

	/**
	 * Begin serving TCP connections.
	 *
	 * Should not be called before info_ has been fully initialized by all involved parties
	 * (tcp_server, udp_server) since no modifications to the stream_info thereafter are permitted.
	 */
	void begin_serving();

	/**
	 * Initiate teardown of IO processes.
	 *
	 * The actual teardown will be performed by the IO thread that runs the operations of
	 * this server.
	 */
	void end_serving();

private:
	friend class client_session;

	/// Start accepting a new connection.
	void accept_next_connection(tcp_acceptor_p &acceptor);

	/// Register an in-flight (active) session with the server (so that we can close it when
	/// a shutdown is requested externally).
	void register_inflight_session(const std::shared_ptr<class client_session> &session);

	void unregister_inflight_session(client_session *session);

	/// Post a close of all in-flight sockets.
	void close_inflight_sessions();

	// data used by the transfer threads
	int chunk_size_; // the chunk size to use (or 0)

	// data shared with the outlet
	stream_info_impl_p info_; // shared stream_info object
	io_context_p io_;	// shared ptr to IO service; ensures that the IO is still around by the time
						// the acceptor needs to be destroyed
	factory_p factory_; // reference to the sample factory (which owns the samples)
	send_buffer_p send_buffer_; // the send buffer, shared with other TCP's and the outlet

	// acceptor socket
	tcp_acceptor_p acceptor_v4_, acceptor_v6_; // our server socket

	// registry of in-flight asessions (for cancellation)
	std::map<void *, std::weak_ptr<client_session>> inflight_;
	std::recursive_mutex inflight_mut_; // mutex protecting the registry from concurrent access

	// some cached data
	std::string shortinfo_msg_; // pre-computed short-info server response
	std::string fullinfo_msg_;	// pre-computed full-info server response
};
} // namespace lsl

#endif
