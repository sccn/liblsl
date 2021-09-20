#include "api_config.h"
#include "lsl_c_api_helpers.hpp"
#include "resolver_impl.h"
#include <cstdint>
#include <exception>
#include <loguru.hpp>
#include <string>
#include <vector>

extern "C" {
#include "api_types.hpp"
// include api_types before public API header
#include "../include/lsl/resolver.h"

using namespace lsl;

LIBLSL_C_API lsl_continuous_resolver lsl_create_continuous_resolver(double forget_after) {
	return resolver_impl::create_resolver(forget_after);
}

LIBLSL_C_API lsl_continuous_resolver lsl_create_continuous_resolver_byprop(
	const char *prop, const char *value, double forget_after) {
	return resolver_impl::create_resolver(forget_after, prop, value);
}

LIBLSL_C_API lsl_continuous_resolver lsl_create_continuous_resolver_bypred(
	const char *pred, double forget_after) {
	return resolver_impl::create_resolver(forget_after, pred, nullptr);
}

LIBLSL_C_API int32_t lsl_resolver_results(
	lsl_continuous_resolver res, lsl_streaminfo *buffer, uint32_t buffer_elements) {
	try {
		std::vector<stream_info_impl> tmp = res->results(buffer_elements);
		// allocate new stream_info_impl's and assign to the buffer
		for (uint32_t k = 0; k < tmp.size(); k++) buffer[k] = new stream_info_impl(tmp[k]);
		return static_cast<int32_t>(tmp.size());
	}
	LSL_RETURN_CAUGHT_EC;
}

LIBLSL_C_API void lsl_destroy_continuous_resolver(lsl_continuous_resolver res) {
	try {
		delete res;
	} catch (std::exception &e) {
		LOG_F(WARNING, "Unexpected during destruction of a continuous_resolver: %s", e.what());
	}
}

LIBLSL_C_API int32_t lsl_resolve_all(
	lsl_streaminfo *buffer, uint32_t buffer_elements, double wait_time) {
	try {
		// create a new resolver
		resolver_impl resolver;
		// invoke it (our only constraint is that the session id shall be the same as ours)
		std::string sess_id = api_config::get_instance()->session_id();
		std::vector<stream_info_impl> tmp =
			resolver.resolve_oneshot((std::string("session_id='") += sess_id) += "'", 0, wait_time);
		// allocate new stream_info_impl's and assign to the buffer
		uint32_t result = buffer_elements < tmp.size() ? buffer_elements : (uint32_t)tmp.size();
		for (uint32_t k = 0; k < result; k++) buffer[k] = new stream_info_impl(tmp[k]);
		return static_cast<int32_t>(result);
	}
	LSL_RETURN_CAUGHT_EC;
}

LIBLSL_C_API int32_t lsl_resolve_byprop(lsl_streaminfo *buffer, uint32_t buffer_elements,
	const char *prop, const char *value, int32_t minimum, double timeout) {
	try {
		std::string query{resolver_impl::build_query(prop, value)};
		auto tmp = resolver_impl().resolve_oneshot(query, minimum, timeout);
		// allocate new stream_info_impl's and assign to the buffer
		uint32_t result = buffer_elements < tmp.size() ? buffer_elements : (uint32_t)tmp.size();
		for (uint32_t k = 0; k < result; k++) buffer[k] = new stream_info_impl(tmp[k]);
		return static_cast<int32_t>(result);
	}
	LSL_RETURN_CAUGHT_EC;
}

LIBLSL_C_API int32_t lsl_resolve_bypred(lsl_streaminfo *buffer, uint32_t buffer_elements,
	const char *pred, int32_t minimum, double timeout) {
	try {
		std::string query{resolver_impl::build_query(pred)};
		auto tmp = resolver_impl().resolve_oneshot(query, minimum, timeout);
		// allocate new stream_info_impl's and assign to the buffer
		uint32_t result = buffer_elements < tmp.size() ? buffer_elements : (uint32_t)tmp.size();
		for (uint32_t k = 0; k < result; k++) buffer[k] = new stream_info_impl(tmp[k]);
		return static_cast<int32_t>(result);
	}
	LSL_RETURN_CAUGHT_EC;
}
}
