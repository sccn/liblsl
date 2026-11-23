#include "common.h"
#include "api_config.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <loguru.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#if defined(_MSC_VER) && _MSC_VER < 1900
#error "MSVC 2013's std::chrono isn't supported. Please upgrade to VS2015 or later"
#endif
#include <windows.h>
// include mmsystem.h after windows.h
#include <mmsystem.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

int64_t lsl::lsl_local_clock_ns() {
	return std::chrono::nanoseconds(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static std::string read_clock_id() {
	std::string id;
#if defined(__linux__)
	// CLOCK_MONOTONIC is per boot, but a time namespace can offset it, so add the namespace inode
	std::ifstream("/proc/sys/kernel/random/boot_id") >> id;
	char ns[64];
	auto len = readlink("/proc/self/ns/time", ns, sizeof(ns) - 1); // "time:[4026531834]"
	if (!id.empty() && len > 0) {
		ns[len] = 0;
		if (const char *start = std::strchr(ns, '['))
			id += '-' + std::string(start + 1, std::strcspn(start + 1, "]"));
	}
#elif defined(__APPLE__)
	char uuid[64] = {0};
	size_t len = sizeof(uuid) - 1;
	if (sysctlbyname("kern.bootsessionuuid", uuid, &len, nullptr, 0) == 0) id = uuid;
#elif defined(_WIN32)
	// QueryPerformanceCounter is per boot: MachineGuid identifies the installation, the boot time
	// (minus the bias accumulated by wall clock adjustments since boot) identifies the boot
	wchar_t guid[64];
	DWORD guid_size = sizeof(guid);
	HKEY key;
	// KEY_WOW64_64KEY: 32-bit processes would otherwise not see the value
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 0,
			KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS)
		return id;
	LONG guid_err =
		RegGetValueW(key, nullptr, L"MachineGuid", RRF_RT_REG_SZ, nullptr, guid, &guid_size);
	RegCloseKey(key);
	if (guid_err != ERROR_SUCCESS) return id;
	struct { // SYSTEM_TIMEOFDAY_INFORMATION
		LARGE_INTEGER BootTime, CurrentTime, TimeZoneBias;
		ULONG TimeZoneId, Reserved;
		ULONGLONG BootTimeBias, SleepTimeBias;
	} tod{};
	using NtQSI = LONG(WINAPI *)(ULONG, PVOID, ULONG, PULONG);
	auto query = reinterpret_cast<NtQSI>(reinterpret_cast<void *>(
		GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation")));
	if (!query || query(3 /* SystemTimeOfDayInformation */, &tod, sizeof(tod), nullptr) != 0)
		return id;
	for (const wchar_t *c = guid; *c; ++c) id += static_cast<char>(*c);
	id += '-' + std::to_string(tod.BootTime.QuadPart - static_cast<LONGLONG>(tod.BootTimeBias));
#endif
	return id;
}

/// 64-bit FNV-1a as 16 hex digits: the same with every compiler and standard library (unlike
/// std::hash), and it keeps the machine and boot identifiers off the wire
static std::string hash_clock_id(const std::string &id) {
	if (id.empty()) return id;
	uint64_t h = 14695981039346656037ULL;
	for (unsigned char c : id) {
		h ^= c;
		h *= 1099511628211ULL;
	}
	char hex[17];
	snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(h));
	return hex;
}

const std::string &lsl::lsl_clock_id() {
	static const std::string id = hash_clock_id(read_clock_id());
	return id;
}

extern "C" {

LIBLSL_C_API int32_t lsl_protocol_version() {
	return lsl::api_config::get_instance()->use_protocol_version();
}

LIBLSL_C_API void lsl_set_config_filename(const char *filename) {
	if (filename) {
		lsl::api_config::set_api_config_filename(filename);
	}
}

LIBLSL_C_API void lsl_set_config_content(const char *content) {
	if (content) {
		lsl::api_config::set_api_config_content(content);
	}
}

LIBLSL_C_API int32_t lsl_library_version() { return LSL_LIBRARY_VERSION; }

LIBLSL_C_API double lsl_local_clock() {
	const auto ns_per_s = 1000000000;
	const auto seconds_since_epoch = std::lldiv(lsl::lsl_local_clock_ns(), ns_per_s);
	/* For large timestamps, converting to double and then dividing by 1e9 loses precision
	   because double has only 53 bits of precision.
	   So we calculate everything we can as integer and only cast to double at the end */
	return static_cast<double>(seconds_since_epoch.quot) +
		   static_cast<double>(seconds_since_epoch.rem) / ns_per_s;
}

LIBLSL_C_API void lsl_destroy_string(char *s) {
	if (s) free(s);
}

LIBLSL_C_API void lsl_destroy_string_array(char **strings, unsigned long count) {
	for (unsigned long k = 0; k < count; k++) lsl_destroy_string(strings[k]);
}

LIBLSL_C_API const char *lsl_last_error(void) {
	thread_local char last_error[LAST_ERROR_SIZE] = {0};
	return last_error;
}
}

// === implementation of misc functions ===

void lsl::ensure_lsl_initialized() {
	static bool is_initialized = false;

	if (!is_initialized) {
		is_initialized = true;

#if LOGURU_DEBUG_LOGGING
		// Initialize loguru, mainly to print stacktraces on segmentation faults
		int argc = 1;
		const char *argv[] = {"liblsl", nullptr};
		loguru::init(argc, const_cast<char **>(argv));
#else
#endif
		LOG_F(INFO, "%s", lsl_library_info());

#ifdef _WIN32
		// if a timer resolution other than 0 is requested (0 means don't override)...
		if (int desired_timer_resolution = lsl::api_config::get_instance()->timer_resolution()) {
			// then override it for the lifetime of this program
			struct override_timer_resolution_until_exit {
				override_timer_resolution_until_exit(int res) : res_(res) { timeBeginPeriod(res_); }
				~override_timer_resolution_until_exit() { timeEndPeriod(res_); }
				int res_;
			};
			static override_timer_resolution_until_exit overrider(desired_timer_resolution);
		}
#endif
	}
}
