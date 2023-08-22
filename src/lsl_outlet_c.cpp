#include "lsl_c_api_helpers.hpp"
#include "stream_outlet_impl.h"
#include <loguru.hpp>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
#include "api_types.hpp"
// include api_types before public API header
#include "../include/lsl/outlet.h"

using namespace lsl;

// boilerplate wrapper code
LIBLSL_C_API lsl_outlet lsl_create_outlet_ex(
	lsl_streaminfo info, int32_t chunk_size, int32_t max_buffered, lsl_transport_options_t flags) {
	return create_object_noexcept<stream_outlet_impl>(*info, chunk_size, max_buffered, flags);
}

LIBLSL_C_API lsl_outlet lsl_create_outlet(
	lsl_streaminfo info, int32_t chunk_size, int32_t max_buffered) {
	return lsl_create_outlet_ex(info, chunk_size, max_buffered, transp_default);
}

LIBLSL_C_API void lsl_destroy_outlet(lsl_outlet out) {
	try {
		delete out;
	} catch (std::exception &e) {
		LOG_F(WARNING, "Unexpected error during deletion of stream outlet: %s", e.what());
	}
}

LIBLSL_C_API int32_t lsl_push_sample_f(lsl_outlet out, const float *data) {
	return out->push_sample_noexcept(data);
}

LIBLSL_C_API int32_t lsl_push_sample_ft(lsl_outlet out, const float *data, double timestamp) {
	return out->push_sample_noexcept(data, timestamp);
}

LIBLSL_C_API int32_t lsl_push_sample_ftp(
	lsl_outlet out, const float *data, double timestamp, int32_t pushthrough) {
	return out->push_sample_noexcept(data, timestamp, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_sample_d(lsl_outlet out, const double *data) {
	return out->push_sample_noexcept(data);
}

LIBLSL_C_API int32_t lsl_push_sample_dt(lsl_outlet out, const double *data, double timestamp) {
	return out->push_sample_noexcept(data, timestamp);
}

LIBLSL_C_API int32_t lsl_push_sample_dtp(
	lsl_outlet out, const double *data, double timestamp, int32_t pushthrough) {
	return out->push_sample_noexcept(data, timestamp, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_sample_l(lsl_outlet out, const int64_t *data) {
	return out->push_sample_noexcept(data);
}

LIBLSL_C_API int32_t lsl_push_sample_lt(lsl_outlet out, const int64_t *data, double timestamp) {
	return out->push_sample_noexcept(data, timestamp);
}

LIBLSL_C_API int32_t lsl_push_sample_ltp(
	lsl_outlet out, const int64_t *data, double timestamp, int32_t pushthrough) {
	return out->push_sample_noexcept(data, timestamp, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_sample_i(lsl_outlet out, const int32_t *data) {
	return out->push_sample_noexcept(data);
}

LIBLSL_C_API int32_t lsl_push_sample_it(lsl_outlet out, const int32_t *data, double timestamp) {
	return out->push_sample_noexcept(data, timestamp);
}

LIBLSL_C_API int32_t lsl_push_sample_itp(
	lsl_outlet out, const int32_t *data, double timestamp, int32_t pushthrough) {
	return out->push_sample_noexcept(data, timestamp, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_sample_s(lsl_outlet out, const int16_t *data) {
	return out->push_sample_noexcept(data);
}

LIBLSL_C_API int32_t lsl_push_sample_st(lsl_outlet out, const int16_t *data, double timestamp) {
	return out->push_sample_noexcept(data, timestamp);
}

LIBLSL_C_API int32_t lsl_push_sample_stp(
	lsl_outlet out, const int16_t *data, double timestamp, int32_t pushthrough) {
	return out->push_sample_noexcept(data, timestamp, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_sample_c(lsl_outlet out, const char *data) {
	return out->push_sample_noexcept(data);
}

LIBLSL_C_API int32_t lsl_push_sample_ct(lsl_outlet out, const char *data, double timestamp) {
	return out->push_sample_noexcept(data, timestamp);
}

LIBLSL_C_API int32_t lsl_push_sample_ctp(
	lsl_outlet out, const char *data, double timestamp, int32_t pushthrough) {
	return out->push_sample_noexcept(data, timestamp, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_sample_v(lsl_outlet out, const void *data) {
	return lsl_push_sample_vtp(out, data, 0.0, true);
}

LIBLSL_C_API int32_t lsl_push_sample_vt(lsl_outlet out, const void *data, double timestamp) {
	return lsl_push_sample_vtp(out, data, timestamp, true);
}

LIBLSL_C_API int32_t lsl_push_sample_vtp(
	lsl_outlet out, const void *data, double timestamp, int32_t pushthrough) {
	try {
		out->push_numeric_raw(data, timestamp, pushthrough != 0);
	} LSL_RETURN_CAUGHT_EC;
}

LIBLSL_C_API int32_t lsl_push_sample_str(lsl_outlet out, const char **data) {
	return lsl_push_sample_strtp(out, data, 0.0, true);
}

LIBLSL_C_API int32_t lsl_push_sample_strt(lsl_outlet out, const char **data, double timestamp) {
	return lsl_push_sample_strtp(out, data, timestamp, true);
}

LIBLSL_C_API int32_t lsl_push_sample_strtp(
	lsl_outlet out, const char **data, double timestamp, int32_t pushthrough) {
	try {
		stream_outlet_impl *outimpl = out;
		std::vector<std::string> tmp;
		for (uint32_t k = 0; k < (uint32_t)outimpl->info().channel_count(); k++)
			tmp.emplace_back(data[k]);
		return outimpl->push_sample_noexcept(&tmp[0], timestamp, pushthrough != 0);
	} catch (std::exception &e) {
		LOG_F(WARNING, "Unexpected error during push_sample: %s", e.what());
		return lsl_internal_error;
	}
}

LIBLSL_C_API int32_t lsl_push_sample_buf(
	lsl_outlet out, const char **data, const uint32_t *lengths) {
	return lsl_push_sample_buftp(out, data, lengths, 0.0, true);
}

LIBLSL_C_API int32_t lsl_push_sample_buft(
	lsl_outlet out, const char **data, const uint32_t *lengths, double timestamp) {
	return lsl_push_sample_buftp(out, data, lengths, timestamp, true);
}

LIBLSL_C_API int32_t lsl_push_sample_buftp(lsl_outlet out, const char **data,
	const uint32_t *lengths, double timestamp, int32_t pushthrough) {
	stream_outlet_impl *outimpl = out;
	// As the number of bytes-per-buffer is the same as the number of chars-per-buffer,
	// we can pass `lengths` through as `bytes`.
	return lsl_push_sample_rawtpn(out, (void **)data, lengths, timestamp, pushthrough,
		(uint32_t)outimpl->info().channel_count());
}

LIBLSL_C_API int32_t lsl_push_sample_rawtpn(lsl_outlet out, void **data, const uint32_t *bytes,
	double timestamp, int32_t pushthrough, uint32_t nbufs) {
	stream_outlet_impl *outimpl = out;
	try {
		if (outimpl->is_sync_blocking()) {
			// Convert input to a vector of asio buffers for a gather-write operation.
			std::vector<asio::const_buffer> bufs;
			bufs.reserve(nbufs);
			for (auto buf_ix = 0; buf_ix < nbufs; buf_ix++) {
				bufs.push_back(asio::buffer(data[buf_ix], bytes[buf_ix]));
			}
			return outimpl->push_sample_gather(bufs, timestamp, pushthrough);
		} else {
			// Make contiguous.
			if (outimpl->info().channel_format() == cft_string) {
				// For strings we place in std::string vector to make sure they are properly
				//  terminated.
				std::vector<std::string> tmp;
				for (uint32_t k = 0; k < nbufs; k++)
					tmp.emplace_back((const char *)data[k], bytes[k]);
				return outimpl->push_sample_noexcept(&tmp[0], timestamp, pushthrough);
			} else {
				// Otherwise we put into new memory block.
				uint32_t total_bytes = 0, byte_offset = 0;
				for (size_t k = 0; k < nbufs; k++) { total_bytes += bytes[k]; }
				char *tmp = (char *)malloc(total_bytes);
				for (size_t k = 0; k < nbufs; k++) {
					memcpy(&tmp[byte_offset], data[k], bytes[k]);
					byte_offset += bytes[k];
				}
				// TODO: I tried passing void buffer but eventually fail because the convert
				// functions
				//  become ambiguous.
				lsl_error_code_t ec;
				switch (outimpl->info().channel_format()) {
				case cft_int8:
					ec = outimpl->push_sample_noexcept((const char *)tmp, timestamp, pushthrough);
				case cft_int16:
					ec =
						outimpl->push_sample_noexcept((const int16_t *)tmp, timestamp, pushthrough);
				case cft_int32:
					ec =
						outimpl->push_sample_noexcept((const int32_t *)tmp, timestamp, pushthrough);
				case cft_int64:
					ec =
						outimpl->push_sample_noexcept((const int64_t *)tmp, timestamp, pushthrough);
				case cft_float32:
					ec = outimpl->push_sample_noexcept((const float *)tmp, timestamp, pushthrough);
				case cft_double64:
					ec = outimpl->push_sample_noexcept((const double *)tmp, timestamp, pushthrough);
				case cft_undefined: ec = lsl_internal_error;
				}
				free(tmp);
				return ec;
			}
		}
	} catch (std::exception &e) {
		LOG_F(WARNING, "Unexpected error during push_sample: %s", e.what());
		if (!outimpl->is_sync_blocking() && outimpl->info().channel_format() != cft_string)
			LOG_F(ERROR, "lsl_push_sample_buftpn only compatible with string type or when outlet "
						 "is using sync writes.");
		return lsl_internal_error;
	}
}

LIBLSL_C_API int32_t lsl_push_chunk_f(
	lsl_outlet out, const float *data, unsigned long data_elements) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements);
}

LIBLSL_C_API int32_t lsl_push_chunk_d(
	lsl_outlet out, const double *data, unsigned long data_elements) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements);
}

LIBLSL_C_API int32_t lsl_push_chunk_l(
	lsl_outlet out, const int64_t *data, unsigned long data_elements) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements);
}

LIBLSL_C_API int32_t lsl_push_chunk_i(
	lsl_outlet out, const int32_t *data, unsigned long data_elements) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements);
}

LIBLSL_C_API int32_t lsl_push_chunk_s(
	lsl_outlet out, const int16_t *data, unsigned long data_elements) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements);
}

LIBLSL_C_API int32_t lsl_push_chunk_c(
	lsl_outlet out, const char *data, unsigned long data_elements) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements);
}

LIBLSL_C_API int32_t lsl_push_chunk_ft(
	lsl_outlet out, const float *data, unsigned long data_elements, double timestamp) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements, timestamp);
}

LIBLSL_C_API int32_t lsl_push_chunk_dt(
	lsl_outlet out, const double *data, unsigned long data_elements, double timestamp) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements, timestamp);
}

LIBLSL_C_API int32_t lsl_push_chunk_lt(
	lsl_outlet out, const int64_t *data, unsigned long data_elements, double timestamp) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements, timestamp);
}

LIBLSL_C_API int32_t lsl_push_chunk_it(
	lsl_outlet out, const int32_t *data, unsigned long data_elements, double timestamp) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements, timestamp);
}

LIBLSL_C_API int32_t lsl_push_chunk_st(
	lsl_outlet out, const int16_t *data, unsigned long data_elements, double timestamp) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements, timestamp);
}

LIBLSL_C_API int32_t lsl_push_chunk_ct(
	lsl_outlet out, const char *data, unsigned long data_elements, double timestamp) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements, timestamp);
}

LIBLSL_C_API int32_t lsl_push_chunk_ftp(lsl_outlet out, const float *data,
	unsigned long data_elements, double timestamp, int32_t pushthrough) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements, timestamp, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_chunk_dtp(lsl_outlet out, const double *data,
	unsigned long data_elements, double timestamp, int32_t pushthrough) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements, timestamp, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_chunk_ltp(lsl_outlet out, const int64_t *data,
	unsigned long data_elements, double timestamp, int32_t pushthrough) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements, timestamp, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_chunk_itp(lsl_outlet out, const int32_t *data,
	unsigned long data_elements, double timestamp, int32_t pushthrough) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements, timestamp, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_chunk_stp(lsl_outlet out, const int16_t *data,
	unsigned long data_elements, double timestamp, int32_t pushthrough) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements, timestamp, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_chunk_ctp(lsl_outlet out, const char *data,
	unsigned long data_elements, double timestamp, int32_t pushthrough) {
	return out->push_chunk_multiplexed_noexcept(data, data_elements, timestamp, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_chunk_ftn(
	lsl_outlet out, const float *data, unsigned long data_elements, const double *timestamps) {
	return out->push_chunk_multiplexed_noexcept(data, timestamps, data_elements);
}

LIBLSL_C_API int32_t lsl_push_chunk_dtn(
	lsl_outlet out, const double *data, unsigned long data_elements, const double *timestamps) {
	return out->push_chunk_multiplexed_noexcept(data, timestamps, data_elements);
}

LIBLSL_C_API int32_t lsl_push_chunk_ltn(
	lsl_outlet out, const int64_t *data, unsigned long data_elements, const double *timestamps) {
	return out->push_chunk_multiplexed_noexcept(data, timestamps, data_elements);
}

LIBLSL_C_API int32_t lsl_push_chunk_itn(
	lsl_outlet out, const int32_t *data, unsigned long data_elements, const double *timestamps) {
	return out->push_chunk_multiplexed_noexcept(data, timestamps, data_elements);
}

LIBLSL_C_API int32_t lsl_push_chunk_stn(
	lsl_outlet out, const int16_t *data, unsigned long data_elements, const double *timestamps) {
	return out->push_chunk_multiplexed_noexcept(data, timestamps, data_elements);
}

LIBLSL_C_API int32_t lsl_push_chunk_ctn(
	lsl_outlet out, const char *data, unsigned long data_elements, const double *timestamps) {
	return out->push_chunk_multiplexed_noexcept(data, timestamps, data_elements);
}

LIBLSL_C_API int32_t lsl_push_chunk_ftnp(lsl_outlet out, const float *data,
	unsigned long data_elements, const double *timestamps, int32_t pushthrough) {
	return out->push_chunk_multiplexed_noexcept(data, timestamps, data_elements, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_chunk_dtnp(lsl_outlet out, const double *data,
	unsigned long data_elements, const double *timestamps, int32_t pushthrough) {
	return out->push_chunk_multiplexed_noexcept(data, timestamps, data_elements, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_chunk_ltnp(lsl_outlet out, const int64_t *data,
	unsigned long data_elements, const double *timestamps, int32_t pushthrough) {
	return out->push_chunk_multiplexed_noexcept(data, timestamps, data_elements, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_chunk_itnp(lsl_outlet out, const int32_t *data,
	unsigned long data_elements, const double *timestamps, int32_t pushthrough) {
	return out->push_chunk_multiplexed_noexcept(data, timestamps, data_elements, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_chunk_stnp(lsl_outlet out, const int16_t *data,
	unsigned long data_elements, const double *timestamps, int32_t pushthrough) {
	return out->push_chunk_multiplexed_noexcept(data, timestamps, data_elements, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_chunk_ctnp(lsl_outlet out, const char *data,
	unsigned long data_elements, const double *timestamps, int32_t pushthrough) {
	return out->push_chunk_multiplexed_noexcept(data, timestamps, data_elements, pushthrough);
}

LIBLSL_C_API int32_t lsl_push_chunk_str(
	lsl_outlet out, const char **data, unsigned long data_elements) {
	return lsl_push_chunk_strtp(out, data, data_elements, 0.0, true);
}

LIBLSL_C_API int32_t lsl_push_chunk_strt(
	lsl_outlet out, const char **data, unsigned long data_elements, double timestamp) {
	return lsl_push_chunk_strtp(out, data, data_elements, timestamp, true);
}

LIBLSL_C_API int32_t lsl_push_chunk_strtp(lsl_outlet out, const char **data,
	unsigned long data_elements, double timestamp, int32_t pushthrough) {
	try {
		std::vector<std::string> tmp;
		for (unsigned long k = 0; k < data_elements; k++) tmp.emplace_back(data[k]);
		if (data_elements) out->push_chunk_multiplexed(&tmp[0], tmp.size(), timestamp, pushthrough);
	}
	LSL_RETURN_CAUGHT_EC;
}

LIBLSL_C_API int32_t lsl_push_chunk_strtn(
	lsl_outlet out, const char **data, unsigned long data_elements, const double *timestamps) {
	return lsl_push_chunk_strtnp(out, data, data_elements, timestamps, true);
}

LIBLSL_C_API int32_t lsl_push_chunk_strtnp(lsl_outlet out, const char **data,
	unsigned long data_elements, const double *timestamps, int32_t pushthrough) {
	try {
		if (data_elements) {
			std::vector<std::string> tmp;
			for (unsigned long k = 0; k < data_elements; k++) tmp.emplace_back(data[k]);
			out->push_chunk_multiplexed_noexcept(&tmp[0], timestamps, data_elements, pushthrough);
		}
	}
	LSL_RETURN_CAUGHT_EC;
}

LIBLSL_C_API int32_t lsl_push_chunk_buf(
	lsl_outlet out, const char **data, const uint32_t *lengths, unsigned long data_elements) {
	return lsl_push_chunk_buftp(out, data, lengths, data_elements, 0.0, true);
}

LIBLSL_C_API int32_t lsl_push_chunk_buft(lsl_outlet out, const char **data, const uint32_t *lengths,
	unsigned long data_elements, double timestamp) {
	return lsl_push_chunk_buftp(out, data, lengths, data_elements, timestamp, true);
}

LIBLSL_C_API int32_t lsl_push_chunk_buftp(lsl_outlet out, const char **data,
	const uint32_t *lengths, unsigned long data_elements, double timestamp, int32_t pushthrough) {
	try {
		std::vector<std::string> tmp;
		for (unsigned long k = 0; k < data_elements; k++) tmp.emplace_back(data[k], lengths[k]);
		if (data_elements) out->push_chunk_multiplexed(&tmp[0], tmp.size(), timestamp, pushthrough);
	}
	LSL_RETURN_CAUGHT_EC;
}

LIBLSL_C_API int32_t lsl_push_chunk_buftn(lsl_outlet out, const char **data,
	const uint32_t *lengths, unsigned long data_elements, const double *timestamps) {
	return lsl_push_chunk_buftnp(out, data, lengths, data_elements, timestamps, true);
}

LIBLSL_C_API int32_t lsl_push_chunk_buftnp(lsl_outlet out, const char **data,
	const uint32_t *lengths, unsigned long data_elements, const double *timestamps,
	int32_t pushthrough) {
	try {
		if (data_elements) {
			std::vector<std::string> tmp;
			for (unsigned long k = 0; k < data_elements; k++) tmp.emplace_back(data[k], lengths[k]);
			out->push_chunk_multiplexed(
				&tmp[0], timestamps, (std::size_t)data_elements, pushthrough);
		}
	}
	LSL_RETURN_CAUGHT_EC;
}

LIBLSL_C_API int32_t lsl_have_consumers(lsl_outlet out) {
	try {
		return out->have_consumers();
	} catch (std::exception &e) {
		LOG_F(WARNING, "Unexpected error in have_consumers: %s", e.what());
		return 1;
	}
}

LIBLSL_C_API int32_t lsl_wait_for_consumers(lsl_outlet out, double timeout) {
	try {
		return out->wait_for_consumers(timeout);
	} catch (std::exception &e) {
		LOG_F(WARNING, "Unexpected error in wait_for_consumers: %s", e.what());
		return 1;
	}
}

LIBLSL_C_API lsl_streaminfo lsl_get_info(lsl_outlet out) {
	return create_object_noexcept<stream_info_impl>(out->info());
}
}
