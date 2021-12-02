#ifndef RESOLVER_IMPL_H
#define RESOLVER_IMPL_H

#include "cancellation.h"
#include "common.h"
#include "forward.h"
#include "stream_info_impl.h"
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/steady_timer.hpp>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using asio::ip::tcp;
using asio::ip::udp;
using err_t = const asio::error_code &;

namespace lsl {
class api_config;

using steady_timer = asio::basic_waitable_timer<asio::chrono::steady_clock, asio::wait_traits<asio::chrono::steady_clock>, asio::io_context::executor_type>;

/// A container for resolve results (map from stream instance UID onto (stream_info,receive-time)).
typedef std::map<std::string, std::pair<stream_info_impl, double>> result_container;

/**
 * A stream resolver object.
 *
 * Maintains the necessary resources for a resolve process,
 * used by the free-standing resolve functions, the continuous_resolver class, and the inlets.
 *
 * A resolver instance can be operated in two different ways:
 *
 * 1) In one shot: The resolver is queried one or more times by calling resolve_oneshot().
 *
 * 2) Continuously: First a background query process is started that keeps updating a results list
 * by calling resolve_continuous() and the list is retrieved in parallel when desired via results().
 * In this case a new resolver instance must be created to issue a new query.
 */
class resolver_impl final : public cancellable_registry {
public:
	/**
	 * Instantiate a new resolver and configure timing parameters.
	 *
	 * @note Resolution logic: If api_config::known_peers is empty, a new multicast wave will be
	 * scheduled every mcast_min_rtt (until a timeout expires or the desired number of streams has
	 * been resolved).If api_config::known_peers is non-empty, a multicast wave and a unicast wave
	 * will be scheduled in alternation.
	 * The spacing between waves will be no shorter than the respective minimum RTTs.
	 * In continuous mode a special, somewhat more lax, set of timings is used (see API config).
	 */
	resolver_impl();

	/**
	 * Build a query string
	 *
	 * @param pred_or_prop an entire predicate if value isn't set or the name of the property, e.g.
	 * `foo='bar'` / `foo` (+value set as "bar")
	 * @param value the value for the property parameter
	 */
	static std::string build_query(const char *pred_or_prop = nullptr, const char *value = nullptr);

	/**
	 * Create a resolver object with optionally a predicate or property + value
	 *
	 * @param forget_after Seconds since last response after which a stream isn't considered to be
	 * online any more.
	 * @param pred_or_prop an entire predicate of value isn't set or the name of the property, e.g.
	 * `foo='bar'` / `foo` (+value set as "bar")
	 * @param value the value for the property parameter
	 * @return A pointer to the resolver on success or nullptr on error
	 */
	static resolver_impl *create_resolver(double forget_after, const char *pred_or_prop = nullptr,
		const char *value = nullptr) noexcept;

	/// Destructor. Cancels any ongoing processes and waits until they finish.
	~resolver_impl() final;

	resolver_impl(const resolver_impl &) = delete;

	/**
	 * Resolve a query string into a list of matching stream_info's on the network.
	 *
	 * Blocks until at least the minimum number of streams has been resolved, or the timeout fires,
	 * or the resolve has been cancelled.
	 * @param query The query string to send (usually a set of conditions on the properties of the
	 * stream info that should be searched, for example "name='BioSemi' and type='EEG'" (without the
	 * outer ""). See lsl_stream_info_matches_query() for the definition of a query.
	 * @param minimum The minimum number of unique streams that should be resolved before this
	 * function may to return.
	 * @param timeout The timeout after which this function is forced to return (even if it did not
	 * produce the desired number of results).
	 * @param minimum_time Search for matching streams for at least this much time (e.g., if
	 * multiple streams may be present).
	 */
	std::vector<stream_info_impl> resolve_oneshot(const std::string &query, int minimum = 0,
		double timeout = FOREVER, double minimum_time = 0.0);

	/**
	 * Starts a background thread that resolves a query string and periodically updates the list of
	 * present streams.
	 *
	 * After this, the resolver can *not* be repurposed for other queries or for
	 * oneshot operation (a new instance needs to be created for that).
	 * @param query The query string to send (usually a set of conditions on the properties of the
	 * stream info that should be searched, for example `name='BioSemi' and type='EEG'`
	 * See stream_info_impl::matches_query() for the definition of a query.
	 * @param forget_after If a stream vanishes from the network (e.g., because it was shut down),
	 * it will be pruned from the list this many seconds after it was last seen.
	 */
	void resolve_continuous(const std::string &query, double forget_after = 5.0);

	/// Get the current set of results (e.g., during continuous operation).
	std::vector<stream_info_impl> results(uint32_t max_results = 4294967295);

	/**
	 * Tear down any ongoing operations and render the resolver unusable.
	 *
	 * This can be used to cancel a blocking resolve_oneshot() from another thread (e.g., to
	 * initiate teardown of the object).
	 */
	void cancel();

	enum class resolver_status {
		empty, started_oneshot, running_continuous
	};

private:
	friend class resolve_attempt_udp;

	/// This function starts a new wave of resolves.
	void next_resolve_wave();

	/// Start a new resolver attempt on the multicast hosts.
	void udp_multicast_burst();

	/// Start a new resolver attempt on the known peers.
	void udp_unicast_burst(err_t err);

	/// Check if cancellation criteria (minimum number of results, timeout) are met
	bool check_cancellation_criteria();

	/// Cancel the currently ongoing resolve, if any.
	void cancel_ongoing_resolve();


	// constants (mostly config-deduced)
	/// pointer to our configuration object
	const api_config *cfg_;
	/// UDP protocols under consideration
	std::vector<udp> udp_protocols_;
	/// the list of multicast endpoints under consideration
	std::vector<udp::endpoint> mcast_endpoints_;
	/// the list of per-host UDP endpoints under consideration
	std::vector<udp::endpoint> ucast_endpoints_;

	// things related to cancellation
	/// if set, no more resolves can be started (destructively cancelled).
	std::atomic<bool> cancelled_;
	/// if set, ongoing operations will finished quickly
	std::atomic<bool> expired_;

	// reinitialized for each query
	resolver_status status{resolver_status::empty};
	/// our current query string
	std::string query_;
	/// the minimum number of results that we want
	int minimum_{0};
	/// forget results that are older than this (continuous operation only)
	double forget_after_;
	/// wait until this point in time before returning results (optional to allow for returning
	/// potentially more than a minimum number of results)
	double wait_until_{0};
	/// whether this is a fast resolve: determines the rate at which the query is repeated
	bool fast_mode_;
	/// results are stored here
	result_container results_;
	/// a mutex that protects the results map
	std::mutex results_mut_;

	// io objects
	/// our IO service
	io_context_p io_;
	/// a thread that runs background IO if we are performing a resolve_continuous
	std::shared_ptr<std::thread> background_io_;
	/// the overall timeout for a query
	steady_timer resolve_timeout_expired_;
	/// a timer that fires when a new wave should be scheduled
	steady_timer wave_timer_;
	/// a timer that fires when the unicast wave should be scheduled
	steady_timer unicast_timer_;
};

} // namespace lsl

#endif
