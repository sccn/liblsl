#include "lsl_c_api_helpers.hpp"
#include "common.h"
#include "stream_info_impl.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <loguru.hpp>
#include <stdexcept>
#include <string>

extern "C" {
#include "api_types.hpp"
// include api_types before public API header
#include "../include/lsl/streaminfo.h"

using namespace lsl;

// boilerplate wrapper code
LIBLSL_C_API lsl_streaminfo lsl_create_streaminfo(const char *name, const char *type,
	int32_t channel_count, double nominal_srate, lsl_channel_format_t channel_format,
	const char *source_id) {
	return create_object_noexcept<stream_info_impl>(
		name, type, channel_count, nominal_srate, channel_format, source_id);
}

LIBLSL_C_API lsl_streaminfo lsl_copy_streaminfo(lsl_streaminfo info) {
	return create_object_noexcept<stream_info_impl>(*info);
}

LIBLSL_C_API void lsl_destroy_streaminfo(lsl_streaminfo info) {
	try {
		delete info;
	} catch (std::exception &e) {
		LOG_F(WARNING, "Unexpected error while destroying a streaminfo: %s", e.what());
	}
}

LIBLSL_C_API const char *lsl_get_type(lsl_streaminfo info) { return info->type().c_str(); }
LIBLSL_C_API const char *lsl_get_name(lsl_streaminfo info) { return info->name().c_str(); }
LIBLSL_C_API int32_t lsl_get_channel_count(lsl_streaminfo info) { return info->channel_count(); }
LIBLSL_C_API double lsl_get_nominal_srate(lsl_streaminfo info) { return info->nominal_srate(); }
LIBLSL_C_API lsl_channel_format_t lsl_get_channel_format(lsl_streaminfo info) {
	return info->channel_format();
}
LIBLSL_C_API const char *lsl_get_source_id(lsl_streaminfo info) {
	return info->source_id().c_str();
}
LIBLSL_C_API int32_t lsl_get_version(lsl_streaminfo info) { return info->version(); }
LIBLSL_C_API double lsl_get_created_at(lsl_streaminfo info) { return info->created_at(); }
LIBLSL_C_API const char *lsl_get_uid(lsl_streaminfo info) { return info->uid().c_str(); }
LIBLSL_C_API const char *lsl_get_session_id(lsl_streaminfo info) {
	return info->session_id().c_str();
}
LIBLSL_C_API const char *lsl_get_hostname(lsl_streaminfo info) { return info->hostname().c_str(); }
LIBLSL_C_API lsl_xml_ptr lsl_get_desc(lsl_streaminfo info) {
	return info->desc().internal_object();
}

LIBLSL_C_API char *lsl_get_xml(lsl_streaminfo info) {
	try {
		std::string tmp = info->to_fullinfo_message();
		char *result = (char *)malloc(tmp.size() + 1);
		if (result == nullptr) {
			LOG_F(ERROR, "Error allocating memory for xmlinfo");
			return nullptr;
		}
		memcpy(result, tmp.data(), tmp.size());
		result[tmp.size()] = '\0';
		return result;
	} catch (std::exception &e) {
		LOG_F(WARNING, "Unexpected error in lsl_get_xml: %s", e.what());
		return nullptr;
	}
}
LIBLSL_C_API int32_t lsl_get_channel_bytes(lsl_streaminfo info) { return info->channel_bytes(); }
LIBLSL_C_API int32_t lsl_get_sample_bytes(lsl_streaminfo info) { return info->sample_bytes(); }

LIBLSL_C_API int32_t lsl_stream_info_matches_query(lsl_streaminfo info, const char *query) {
	return info->matches_query(query);
}

LIBLSL_C_API lsl_streaminfo lsl_streaminfo_from_xml(const char *xml) {
	try {
		auto *impl = new stream_info_impl();
		impl->from_fullinfo_message(xml);
		return impl;
	} catch (std::exception &e) {
		LOG_F(WARNING, "Unexpected error during streaminfo construction: %s", e.what());
		return nullptr;
	}
}
}
