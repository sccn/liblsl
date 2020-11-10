#include "api_config.h"
#include "common.h"
#include "inireader.h"
#include <algorithm>
#include <fstream>
#include <mutex>

using namespace lsl;

/// Substitute the "~" character by the full home directory (according to environment variables).
std::string expand_tilde(const std::string &filename) {
	if (!filename.empty() && filename[0] == '~') {
		std::string homedir;
		if (getenv("HOME"))
			homedir = getenv("HOME");
		else if (getenv("USERPROFILE"))
			homedir = getenv("USERPROFILE");
		else if (getenv("HOMEDRIVE") && getenv("HOMEPATH"))
			homedir = std::string(getenv("HOMEDRIVE")) + getenv("HOMEPATH");
		else {
			LOG_F(WARNING, "Cannot determine the user's home directory; config files in the home "
						   "directory will not be discovered.");
			return filename;
		}
		return homedir + filename.substr(1);
	}
	return filename;
}

/// Parse a set specifier (a string of the form {a, b, c, ...}) into a vector of strings.
static std::vector<std::string> parse_set(const std::string &setstr) {
	std::vector<std::string> result;
	if ((setstr.size() > 2) && setstr[0] == '{' && setstr[setstr.size() - 1] == '}') {
		result = splitandtrim(setstr.substr(1, setstr.size() - 2), ',', false);
	}
	return result;
}

/// Returns true if the file exists and is openable for reading
bool file_is_readable(const std::string &filename) {
	std::ifstream f(filename);
	return f.good();
}

api_config::api_config() {
	// for each config file location under consideration...
	std::vector<std::string> filenames;
	if (getenv("LSLAPICFG")) {
		std::string envcfg(getenv("LSLAPICFG"));
		if (!file_is_readable(envcfg))
			LOG_F(ERROR, "LSLAPICFG file %s not found", envcfg.c_str());
		else
			filenames.insert(filenames.begin(), envcfg);
	}
	filenames.emplace_back("lsl_api.cfg");
	filenames.push_back(expand_tilde("~/lsl_api/lsl_api.cfg"));
	filenames.emplace_back("/etc/lsl_api/lsl_api.cfg");
	for (auto &filename : filenames) {
		try {
			if (file_is_readable(filename)) {
				// try to load it if the file exists
				load_from_file(filename);
				// successful: finished
				return;
			}
		} catch (std::exception &e) {
			LOG_F(ERROR, "Error trying to load config file %s: %s", filename.c_str(), e.what());
		}
	}
	// unsuccessful: load default settings
	load_from_file();
}


void api_config::load_from_file(const std::string &filename) {
	try {
		INI pt;
		if (!filename.empty()) {
			std::ifstream infile(filename);
			if (infile.good()) {
				LOG_F(INFO, "Loading configuration from %s", filename.c_str());
				pt.load(infile);
			}
		}

		// read out the [ports] parameters
		multicast_port_ = pt.get("ports.MulticastPort", 16571);
		base_port_ = pt.get("ports.BasePort", 16572);
		port_range_ = pt.get("ports.PortRange", 32);
		allow_random_ports_ = pt.get("ports.AllowRandomPorts", true);
		std::string ipv6_str = pt.get("ports.IPv6",
#ifdef __APPLE__
			"disable"); // on Mac OS (10.7) there's a bug in the IPv6 implementation that breaks LSL
						// when it tries to use both v4 and v6
#else
			"allow");
#endif
		allow_ipv4_ = true;
		allow_ipv6_ = true;
		// fix some common mis-spellings
		if (ipv6_str == "disabled" || ipv6_str == "disable")
			allow_ipv6_ = false;
		else if (ipv6_str == "allowed" || ipv6_str == "allow")
			allow_ipv6_ = true;
		else if (ipv6_str == "forced" || ipv6_str == "force")
			allow_ipv4_ = false;
		else
			throw std::runtime_error("Unsupported setting for the IPv6 parameter.");

		// read the [multicast] parameters
		resolve_scope_ = pt.get("multicast.ResolveScope", "site");
		listen_address_ = pt.get("multicast.ListenAddress", "");
		// Note about multicast addresses: IPv6 multicast addresses should be
		// FF0x::1 (see RFC2373, RFC1884) or a predefined multicast group
		std::string ipv6_multicast_group =
			pt.get("multicast.IPv6MulticastGroup", "113D:6FDD:2C17:A643:FFE2:1BD1:3CD2");
		std::vector<std::string> machine_group =
			parse_set(pt.get("multicast.MachineAddresses", "{127.0.0.1}"));
		// 224.0.0.1 is the group for all directly connected hosts (RFC1112)
		std::vector<std::string> link_group = parse_set(
			pt.get("multicast.LinkAddresses", "{255.255.255.255, 224.0.0.1, 224.0.0.183}"));
		// Multicast groups defined by the organization (and therefore subject
		// to filtering / forwarding are in the 239.192.0.0/14 subnet (RFC2365)
		std::vector<std::string> site_group =
			parse_set(pt.get("multicast.SiteAddresses", "{239.255.172.215}"));
		// Organization groups use the same broadcast addresses (IPv4), but
		// have a larger TTL. On the network site, it requires the routers
		// to forward the broadcast packets (both IGMP and UDP)
		std::vector<std::string> organization_group =
			parse_set(pt.get("multicast.OrganizationAddresses", "{}"));
		std::vector<std::string> global_group =
			parse_set(pt.get("multicast.GlobalAddresses", "{}"));
		enum { machine = 0, link, site, organization, global } scope;
		// construct list of addresses & TTL according to the ResolveScope.
		if (resolve_scope_ == "machine")
			scope = machine;
		else if (resolve_scope_ == "link")
			scope = link;
		else if (resolve_scope_ == "site")
			scope = site;
		else if (resolve_scope_ == "organization")
			scope = organization;
		else if (resolve_scope_ == "global")
			scope = global;
		else
			throw std::runtime_error("This ResolveScope setting is unsupported.");

		multicast_addresses_.insert(
			multicast_addresses_.end(), machine_group.begin(), machine_group.end());
		multicast_ttl_ = 0;

		if (scope >= link) {
			multicast_addresses_.insert(
				multicast_addresses_.end(), link_group.begin(), link_group.end());
			multicast_addresses_.push_back("FF02:" + ipv6_multicast_group);
			multicast_ttl_ = 1;
		}
		if (scope >= site) {
			multicast_addresses_.insert(
				multicast_addresses_.end(), site_group.begin(), site_group.end());
			multicast_addresses_.push_back("FF05:" + ipv6_multicast_group);
			multicast_ttl_ = 24;
		}
		if (scope >= organization) {
			multicast_addresses_.insert(
				multicast_addresses_.end(), organization_group.begin(), organization_group.end());
			multicast_addresses_.push_back("FF08:" + ipv6_multicast_group);
			multicast_ttl_ = 32;
		}
		if (scope >= global) {
			multicast_addresses_.insert(
				multicast_addresses_.end(), global_group.begin(), global_group.end());
			multicast_addresses_.push_back("FF0E:" + ipv6_multicast_group);
			multicast_ttl_ = 255;
		}

		// apply overrides, if any
		int ttl_override = pt.get("multicast.TTLOverride", -1);
		std::vector<std::string> address_override =
			parse_set(pt.get("multicast.AddressesOverride", "{}"));
		if (ttl_override >= 0) multicast_ttl_ = ttl_override;
		if (!address_override.empty()) multicast_addresses_ = address_override;

		// read the [lab] settings
		known_peers_ = parse_set(pt.get("lab.KnownPeers", "{}"));
		session_id_ = pt.get("lab.SessionID", "default");

		// read the [tuning] settings
		use_protocol_version_ = std::min(
			LSL_PROTOCOL_VERSION, pt.get("tuning.UseProtocolVersion", LSL_PROTOCOL_VERSION));
		watchdog_check_interval_ = pt.get("tuning.WatchdogCheckInterval", 15.0);
		watchdog_time_threshold_ = pt.get("tuning.WatchdogTimeThreshold", 15.0);
		multicast_min_rtt_ = pt.get("tuning.MulticastMinRTT", 0.5);
		multicast_max_rtt_ = pt.get("tuning.MulticastMaxRTT", 3.0);
		unicast_min_rtt_ = pt.get("tuning.UnicastMinRTT", 0.75);
		unicast_max_rtt_ = pt.get("tuning.UnicastMaxRTT", 5.0);
		continuous_resolve_interval_ = pt.get("tuning.ContinuousResolveInterval", 0.5);
		timer_resolution_ = pt.get("tuning.TimerResolution", 1);
		max_cached_queries_ = pt.get("tuning.MaxCachedQueries", 100);
		time_update_interval_ = pt.get("tuning.TimeUpdateInterval", 2.0);
		time_update_minprobes_ = pt.get("tuning.TimeUpdateMinProbes", 6);
		time_probe_count_ = pt.get("tuning.TimeProbeCount", 8);
		time_probe_interval_ = pt.get("tuning.TimeProbeInterval", 0.064);
		time_probe_max_rtt_ = pt.get("tuning.TimeProbeMaxRTT", 0.128);
		outlet_buffer_reserve_ms_ = pt.get("tuning.OutletBufferReserveMs", 5000);
		outlet_buffer_reserve_samples_ = pt.get("tuning.OutletBufferReserveSamples", 128);
		inlet_buffer_reserve_ms_ = pt.get("tuning.InletBufferReserveMs", 5000);
		inlet_buffer_reserve_samples_ = pt.get("tuning.InletBufferReserveSamples", 128);
		smoothing_halftime_ = pt.get("tuning.SmoothingHalftime", 90.0f);
		force_default_timestamps_ = pt.get("tuning.ForceDefaultTimestamps", false);

		// read the [log] settings
		int log_level = pt.get("log.level", (int) loguru::Verbosity_INFO);
		if (log_level < -3 || log_level > 9)
			throw std::runtime_error("Invalid log.level (valid range: -3 to 9");

		std::string log_file = pt.get("log.file", "");
		if (!log_file.empty()) {
			loguru::add_file(log_file.c_str(), loguru::Append, log_level);
			// don't duplicate log to stderr
			loguru::g_stderr_verbosity = -9;
		} else
			loguru::g_stderr_verbosity = log_level;

	} catch (std::exception &e) {
		LOG_F(ERROR, "Error parsing config file '%s': '%s', rolling back to defaults",
			filename.c_str(), e.what());
		// any error: assign defaults
		load_from_file();
		// and rethrow
		throw e;
	}
}

static std::once_flag api_config_once_flag;

const api_config *api_config::get_instance() {
	std::call_once(api_config_once_flag, []() { api_config::get_instance_internal(); });
	return get_instance_internal();
}

api_config *api_config::get_instance_internal() {
	static api_config cfg;
	return &cfg;
}
