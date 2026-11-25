#ifndef INLET_CONNECTION_H
#define INLET_CONNECTION_H

#include "cancellation.h"
#include "resolver_impl.h"
#include "stream_info_impl.h"
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>

/* shared_mutex was added in C++17 so we fall back to a plain mutex when
building for C++11 / C++14 or MSVC <= 2019 */
#if __cplusplus >= 201703L || _MSC_VER >= 1925
#include <shared_mutex>
using shared_mutex_t = std::shared_mutex;
using shared_lock_t = std::shared_lock<std::shared_mutex>;
#else
using shared_mutex_t = std::mutex;
using shared_lock_t = std::unique_lock<std::mutex>;
#endif

using unique_lock_t = std::unique_lock<shared_mutex_t>;

using asio::ip::tcp;
using asio::ip::udp;

namespace lsl {

/**
 * An inlet connection encapsulates the inlet's connection endpoint and provides a recovery
 * mechanism in case the connection breaks down (e.g., due to a remote computer crash).
 *
 * When a client of the connection (one of the other inlet components) experiences a connection loss
 * it invokes the function try_recover_from_error() which attempts to update the endpoint to a valid
 * state (possible once the stream is back online).
 *
 * Since in some cases a client might not be able to detect a connection loss and so would stall
 * forever, the inlet_connection maintains a watchdog thread that periodically checks and recovers
 * the connection state. If a regular-rate stream delivers no data but is still served at the same
 * endpoint, the watchdog assumes the connection died silently and makes the clients reconnect.
 *
 * Internally the recovery works by using the resolver to find the desired stream on the network
 * again and updating the endpoint information if it has changed.
 */
class inlet_connection : public cancellable_registry {
public:
	/**
	 * Construct a new inlet connection.
	 * @param info A stream info object (either coming from one of the resolver functions or
	 *constructed manually).
	 * @param recover Try to silently recover lost streams that are recoverable (= those that that
	 *have a unique source_id set). In all other cases (recover is false or the stream is not
	 *recoverable) the stream is declared lost in case of a connection breakdown.
	 */
	inlet_connection(const stream_info_impl &info, bool recover = true);

	/// Destructor: signals shutdown and joins the watchdog thread if it is still running
	/// (e.g., when disengage() did not complete).
	~inlet_connection() override;

	/**
	 * Prepare the connection and its auto-recovery thread.
	 *
	 * This is called once by the inlet after all other initialization.
	 */
	void engage();

	/**
	 * Shut down the connection and all its running operations.
	 *
	 * This is called once by the inlet before any component is destroyed to ensure orderly
	 * shutdown.
	 */
	void disengage();


	// === endpoint information (might change if the connection is rehosted) ===

	/// Get the current TCP endpoint from the info (according to our configured protocol).
	tcp::endpoint get_tcp_endpoint();
	/// Get the current UDP endpoint from the info (according to our configured protocol).
	udp::endpoint get_udp_endpoint();

	/// Get the UDP protocol type.
	udp udp_protocol() const { return udp_protocol_; }

	// === connection status info ===

	/// True if the connection has been irretrievably lost.
	bool lost() const { return lost_; }

	/// True if the connection is being shut down.
	bool shutdown() const { return shutdown_; }


	// === recovery control ===

	/**
	 * Try to recover the connection.
	 *
	 * This either blocks until it succeeds or declares the connection as lost (if recovery is
	 * disabled), and throws a lost error.
	 * An optional cancellation flag stops waiting without declaring the connection lost.
	 * Only call this when the connection is found to have broken down (e.g., socket error).
	 */
	void try_recover_from_error(const std::atomic<bool> *cancel = nullptr);


	// === client status info ===

	/// Indicate that a transmission is now active and requesting the watchdog.
	/// The recovery watchdog will be inactive while no transmission is requested.
	/// @param data true for a data transmission, which reports received samples with
	/// update_receive_time() and is reconnected by the watchdog if a regular-rate stream stalls.
	void acquire_watchdog(bool data = false);

	/// Indicate that a transmission has just completed.
	/// The recovery watchdog will be inactive while no transmission is requested.
	/// @param data must match the argument of the corresponding acquire_watchdog() call.
	void release_watchdog(bool data = false);

	/// Inform the connection that content was received from the source (using lsl::lsl_clock()).
	/// If a sufficient amount of time has passed since the last call the watchdog thread will
	/// try to recover the connection.
	void update_receive_time(double t);

	/// Register a condition variable that should be notified when a connection is lost
	void register_onlost(void *id, std::condition_variable *cond);

	/// Unregister a condition variable from the set that is notified on connection loss
	void unregister_onlost(void *id);

	/// Register a callback function that shall be called when a recovery has been performed
	void register_onrecover(void *id, const std::function<void()> &func);

	/// Unregister a recovery callback function
	void unregister_onrecover(void *id);

	/// Register a callback that cancels a data transmission whose connection has stalled.
	/// It is called from the watchdog thread when a regular-rate stream stops delivering data
	/// although its outlet is still online, see reconnect_if_stalled().
	void register_onstall(void *id, const std::function<void()> &func);

	/// Unregister a stall callback function
	void unregister_onstall(void *id);


	// === misc properties of the connected stream ===

	/// Get info about the connected stream's type/format.
	/// This information is constant and will never change over the lifetime of the connection.
	const stream_info_impl &type_info() const { return type_info_; }

	/// Get the current stream instance UID (which would be different after a crash and restart of
	/// the data source).
	std::string current_uid();

	/// Get the nominal srate of the endpoint; we assume that this might possibly change between
	/// crashes/restarts of the data source under some circumstances (although such behavior would
	/// be strongly discouraged).
	double current_srate();


private:
	/// A thread that periodically checks whether the connection should be recovered.
	void watchdog_thread();

	/**
	 * A (potentially speculative) resolve-and-recover operation.
	 * @return true if the stream is still served at the current endpoint (same UID), false if
	 * the endpoint was switched to a new instance of the stream, recovery is disabled, or the
	 * attempt was cancelled or failed.
	 */
	bool try_recover(const std::atomic<bool> *cancel = nullptr);

	/**
	 * Reconnect the data transmission if a regular-rate stream is stalled even though it is
	 * still served at the current endpoint.
	 *
	 * This catches data connections that died without an error, e.g. because the network
	 * dropped packets without the operating system sending or receiving a FIN or RST
	 * (a half-open connection). The data receiver would otherwise block in a read forever.
	 *
	 * A stream counts as stalled when no sample arrived for longer than the watchdog threshold
	 * and at least min_stalled_samples sample intervals. A paused outlet looks the same, so
	 * this reconnects at most once per silence episode: it is re-armed only when a sample
	 * arrives (update_receive_time()) or a new data transmission starts.
	 */
	void reconnect_if_stalled();

	/// Minimum number of missed sample intervals before a regular-rate stream counts as stalled
	static constexpr double min_stalled_samples = 10.;

	/// Sets the endpoints from a stream info considering a previous connection
	bool set_protocols(const stream_info_impl &info, bool prefer_v6);

	// core connection properties
	/// static/read-only information of the stream (type & format)
	const stream_info_impl type_info_;
	/// the volatile information of the stream (addresses + ports); protected by a read/write mutex
	stream_info_impl host_info_;
	/// a mutex to protect the state of the host_info (single-write/multiple-reader)
	shared_mutex_t host_info_mut_;
	/// the TCP protocol used (according to api_config)
	tcp tcp_protocol_;
	/// the UDP protocol used (according to api_config)
	udp udp_protocol_;
	/// whether we would try to recover the stream if it is lost
	bool recovery_enabled_;
	/// is the stream irrecoverably lost (set by try_recover_from_error if recovery is disabled)
	std::atomic<bool> lost_;

	/// internal watchdog thread (to detect dead connections), re-resolves the current connection
	/// speculatively
	std::thread watchdog_thread_;

	// things related to the shutdown condition
	/// indicates to threads that we're shutting down
	std::atomic<bool> shutdown_;
	/// a mutex to protect the shutdown state
	std::mutex shutdown_mut_;
	/// condition variable indicating that we're shutting down
	std::condition_variable shutdown_cond_;

	// things related to recovery
	/// our resolver, in case we need it
	resolver_impl resolver_;
	/// we allow only one recovery operation at a time
	std::timed_mutex recovery_mut_;

	// client status info for recovery & notification purposes
	/// a group of condition variables that should be notified when the connection is lost
	std::map<void *, std::condition_variable *> onlost_;
	/// a group of callback functions that should be invoked once the connection has been recovered
	std::map<void *, std::function<void()>> onrecover_;
	/// the last time when we received data from the server
	double last_receive_time_;
	/// the number of currently active transmissions (data, info or time)
	int active_transmissions_;
	/// the number of currently active data transmissions (a subset of active_transmissions_)
	int active_data_transmissions_;
	/// whether a stalled regular-rate stream may be reconnected; cleared by a stall reconnect
	/// and set again once a sample arrives, so a paused outlet isn't reconnected over and over
	bool stall_reconnect_armed_;
	/// protects the client status info
	std::mutex client_status_mut_;
	/// protects the onrecover callback map
	std::mutex onrecover_mut_;
	/// callbacks that cancel stalled data transmissions
	std::map<void *, std::function<void()>> onstall_;
	/// protects the onstall callback map
	std::mutex onstall_mut_;
};
} // namespace lsl

#endif
