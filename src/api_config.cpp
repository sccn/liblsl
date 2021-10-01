#include "api_config.h"
#include "common.h"
#include "util/inireader.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <functional>
#include <loguru.hpp>
#include <map>
#include <mutex>
#include <stdexcept>

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

api_config::api_config(bool skip_load_settings) {
	if (skip_load_settings) return;

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
	if(filename.empty()) return;
	try {
		std::ifstream infile(filename);
		if (!infile.good())
			return;

		load_ini_file(infile, [this](const std::string &section, const std::string &key,
								  const std::string &value) { set_option(section, key, value); });



		// log config filename only after setting the verbosity level
		if (!filename.empty())
			LOG_F(INFO, "Configuration loaded from %s", filename.c_str());
		else
			LOG_F(INFO, "Loaded default config");

	} catch (std::exception &e) {
		LOG_F(ERROR, "Error parsing config file '%s': '%s', rolling back to defaults",
			filename.c_str(), e.what());
		// any error: assign defaults
		load_from_file();
		// and rethrow
		throw e;
	}
}

void lsl::api_config::update_multicast_groups() {
	int ttls[] = {0, 1, 24, 32, 255};
	multicast_ttl_ = ttls[resolve_scope_];

	const char IPv6_multicast_scopes[] = {'\0', '2', '5', '8', 'E'};

	multicast_addresses_.clear();
	std::string v6_multicast_group = "FF0?" + ipv6_multicast_group_;
	for (int scope = machine; scope <= resolve_scope_; ++scope) {
		const auto &grpaddrs = multicast_group_addresses_[scope];
		multicast_addresses_.insert(multicast_addresses_.end(), grpaddrs.begin(), grpaddrs.end());
		if (IPv6_multicast_scopes[scope]) {
			v6_multicast_group[4] = IPv6_multicast_scopes[scope];
			multicast_addresses_.push_back(v6_multicast_group);
		}
	}
}

template <typename T>
inline void set_from_string(
	T &var, const std::string &val, T (*converter)(const std::string &) = from_string<T>) {
	var = converter(val);
}

bool lsl::api_config::set_option(
	const std::string &section, const std::string &key, const std::string &value) {

	// [ports]
	if (section == "ports") {
		if (key == "MulticastPort")
			set_from_string(multicast_port_, value);
		else if (key == "BasePort")
			set_from_string(base_port_, value);
		else if (key == "PortRange")
			set_from_string(port_range_, value);
		else if (key == "AllowRandomPorts")
			set_from_string(allow_random_ports_, value);
		else if (key == "IPv6") {
			allow_ipv4_ = true;
			allow_ipv6_ = true;
			if (value == "disabled" || value == "disable")
				allow_ipv6_ = false;
			else if (value == "allowed" || value == "allow")
				allow_ipv6_ = true;
			else if (value == "forced" || value == "force")
				allow_ipv4_ = false;
			else
				throw std::runtime_error("Unsupported setting for the IPv6 parameter.");
		} else if (key == "MachineAddresses")
			multicast_group_addresses_[machine] = parse_set(value);
		else if (key == "LinkAddresses")
			multicast_group_addresses_[link] = parse_set(value);
		else if (key == "SiteAddresses")
			multicast_group_addresses_[site] = parse_set(value);
		else if (key == "OrganizationAddresses")
			multicast_group_addresses_[organization] = parse_set(value);
		else if (key == "GlobalAddresses")
			multicast_group_addresses_[global] = parse_set(value);
	}
	// [multicast]
	else if (section == "multicast") {
		if (key == "ResolveScope") {
			if (value == "machine")
				resolve_scope_ = machine;
			else if (value == "link")
				resolve_scope_ = link;
			else if (value == "site")
				resolve_scope_ = site;
			else if (value == "organization")
				resolve_scope_ = organization;
			else if (value == "global")
				resolve_scope_ = global;
			else
				throw std::runtime_error("This ResolveScope setting is unsupported.");
			update_multicast_groups();
		} else if (key == "ListenAddress")
			listen_address_ = value;
		else if (value == "TTLOverride") {
			// apply overrides, if any
			int ttl_override = from_string<int>(value);
			if (ttl_override < 0 || ttl_override > 255)
				throw std::runtime_error("Invalid TTLOverride value");
			multicast_ttl_ = ttl_override;
		}
		/*std::vector<std::string> address_override =
			parse_set(pt.get("multicast.AddressesOverride", "{}"));
		if (!address_override.empty()) multicast_addresses_ = address_override;*/

	}
	// [lab]
	else if (section == "lab") {
		if (key == "KnownPeers")
			known_peers_ = parse_set(value);
		else if (key == "SessionID")
			session_id_ = value;
	}
	// [log]
	else if(section=="log") {
		// read the [log] settings
		if(key=="level") {
			int8_t log_level = from_string<int>(value);
			if (log_level < -3 || log_level > 9)
				throw std::runtime_error("Invalid log.level (valid range: -3 to 9");
			log_level_ = log_level;
		}
		else if(key == "file") {
		if (!value.empty()) {
			loguru::add_file(value.c_str(), loguru::Append, log_level_);
			// don't duplicate log to stderr
			loguru::g_stderr_verbosity = -9;
		} else
			loguru::g_stderr_verbosity = log_level_;
		}

	}
	// [tuning]
	else if (section == "tuning") {
		if (key == "UseProtocolVersion") {
			int tmpversion = from_string<int>(value);
			if (tmpversion > LSL_PROTOCOL_VERSION)
				throw std::runtime_error("Requested protocol version " + value +
										 " too new; this library supports up to " +
										 std::to_string(LSL_PROTOCOL_VERSION));
			use_protocol_version_ = tmpversion;
		} else if (key == "ContinuousResolveInterval")
			set_from_string(continuous_resolve_interval_, value);
		else if (key == "ForceDefaultTimestamps")
			set_from_string(force_default_timestamps_, value);
		else if (key == "InletBufferReserveMs")
			set_from_string(inlet_buffer_reserve_ms_, value);
		else if (key == "InletBufferReserveSamples")
			set_from_string(inlet_buffer_reserve_samples_, value);
		else if (key == "MaxCachedQueries")
			set_from_string(max_cached_queries_, value);
		else if (key == "MulticastMaxRTT")
			set_from_string(multicast_max_rtt_, value);
		else if (key == "MulticastMinRTT")
			set_from_string(multicast_min_rtt_, value);
		else if (key == "OutletBufferReserveMs")
			set_from_string(outlet_buffer_reserve_ms_, value);
		else if (key == "OutletBufferReserveSamples")
			set_from_string(outlet_buffer_reserve_samples_, value);
		else if (key == "ReceiveSocketBufferSize")
			set_from_string(socket_receive_buffer_size_, value);
		else if (key == "SendSocketBufferSize")
			set_from_string(socket_send_buffer_size_, value);
		else if (key == "SmoothingHalftime")
			set_from_string(smoothing_halftime_, value);
		else if (key == "TimeProbeCount")
			set_from_string(time_probe_count_, value);
		else if (key == "TimeProbeInterval")
			set_from_string(time_probe_interval_, value);
		else if (key == "TimeProbeMaxRTT")
			set_from_string(time_probe_max_rtt_, value);
		else if (key == "TimeUpdateInterval")
			set_from_string(time_update_interval_, value);
		else if (key == "TimeUpdateMinProbes")
			set_from_string(time_update_minprobes_, value);
		else if (key == "TimerResolution")
			set_from_string(timer_resolution_, value);
		else if (key == "UnicastMaxRTT")
			set_from_string(unicast_max_rtt_, value);
		else if (key == "UnicastMinRTT")
			set_from_string(unicast_min_rtt_, value);
		else if (key == "WatchdogCheckInterval")
			set_from_string(watchdog_check_interval_, value);
		else if (key == "WatchdogTimeThreshold")
			set_from_string(watchdog_time_threshold_, value);
	}
	else {
		LOG_F(ERROR, "Unknown configuration option %s.%s = %s", section.c_str(), key.c_str(), value.c_str());
		return false;
	}

	return true;
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
