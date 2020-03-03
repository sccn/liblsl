#include "resolver_impl.h"
#include "api_config.h"

extern "C" {
#include "api_types.hpp"

#include "../include/lsl/resolver.h"
// === implementation of the continuous_resolver class ===

using namespace lsl;

/**
* Construct a new continuous_resolver that resolves all streams on the network. 
* This is analogous to the functionality offered by the free function resolve_streams().
* @param forget_after When a stream is no longer visible on the network (e.g., because it was shut down),
*				      this is the time in seconds after which it is no longer reported by the resolver.
*					  The recommended default value is 5.0.
*/
LIBLSL_C_API lsl_continuous_resolver lsl_create_continuous_resolver(double forget_after) {
	return resolver_impl::create_resolver(forget_after);
}

/**
* Construct a new continuous_resolver that resolves all streams with a specific value for a given property.
* This is analogous to the functionality provided by the free function resolve_stream(prop,value).
* @param prop The stream_info property that should have a specific value (e.g., "name", "type", "source_id", or "desc/manufaturer").
* @param value The string value that the property should have (e.g., "EEG" as the type property).
* @param forget_after When a stream is no longer visible on the network (e.g., because it was shut down),
*				      this is the time in seconds after which it is no longer reported by the resolver.
*					  The recommended default value is 5.0.
*/
LIBLSL_C_API lsl_continuous_resolver lsl_create_continuous_resolver_byprop(const char *prop, const char *value, double forget_after) {
	return resolver_impl::create_resolver(forget_after, prop, value);
}

/**
* Construct a new continuous_resolver that resolves all streams that match a given predicate.
* This is analogous to the functionality provided by the free function resolve_stream(pred).
* @param pred The predicate string, e.g. "name='BioSemi'" or "type='EEG' and starts-with(name,'BioSemi') and count(info/desc/channel)=32"
* @param forget_after When a stream is no longer visible on the network (e.g., because it was shut down),
*				      this is the time in seconds after which it is no longer reported by the resolver.
*					  The recommended default value is 5.0.
*/
LIBLSL_C_API lsl_continuous_resolver lsl_create_continuous_resolver_bypred(const char *pred, double forget_after) {
	return resolver_impl::create_resolver(forget_after, pred, nullptr);
}

/**
* Obtain the set of currently present streams on the network (i.e. resolve result).
* @param buffer A user-allocated buffer to hold the resolve results.
*				Note: it is the user's responsibility to either destroy the resulting streaminfo 
*				objects or to pass them back to the LSL during during creation of an inlet.
* @param buffer_elements The user-provided buffer length.
* @return The number of results written into the buffer (never more than the provided # of slots) 
*		  or a negative number if an error has occurred (values corresponding to lsl_error_code_t).
*/
LIBLSL_C_API int32_t lsl_resolver_results(lsl_continuous_resolver res, lsl_streaminfo *buffer, uint32_t buffer_elements) {
	try {
		std::vector<stream_info_impl> tmp = res->results(buffer_elements);
		// allocate new stream_info_impl's and assign to the buffer
		for (uint32_t k = 0; k < tmp.size(); k++) buffer[k] = new stream_info_impl(tmp[k]);
		return tmp.size();
	} catch(std::exception &e) {
		LOG_F(WARNING, "Unexpected error querying lsl_resolver_results: %s", e.what());
		return lsl_internal_error;
	}
}

/** 
* Destructor for the continuous resolver.
*/
LIBLSL_C_API void lsl_destroy_continuous_resolver(lsl_continuous_resolver res) {
	try {
		delete res;
	} catch(std::exception &e) {
		LOG_F(WARNING, "Unexpected during destruction of a continuous_resolver: %s", e.what());
	}
}

/**
* Resolve all streams on the network.
* This function returns all currently available streams from any outlet on the network.
* The network is usually the subnet specified at the local router, but may also include
* a multicast group of machines (given that the network supports it), or list of hostnames.
* These details may optionally be customized by the experimenter in a configuration file
* (see Network Connectivity in the LSL wiki).
* This is the default mechanism used by the browsing programs and the recording program.
* @param buffer A user-allocated buffer to hold the resolve results.
*				Note: it is the user's responsibility to either destroy the resulting streaminfo
*				objects or to pass them back to the LSL during during creation of an inlet.
* @param buffer_elements The user-provided buffer length.
* @param wait_time The waiting time for the operation, in seconds, to search for streams.
*				   Warning: If this is too short (<0.5s) only a subset (or none) of the
*							outlets that are present on the network may be returned.
* @return The number of results written into the buffer (never more than the provided # of slots) or a negative number if an
*		  error has occurred (values corresponding to lsl_error_code_t).
*/
LIBLSL_C_API int32_t lsl_resolve_all(lsl_streaminfo *buffer, uint32_t buffer_elements, double wait_time) {
	try {
		// create a new resolver
		resolver_impl resolver;
		// invoke it (our only constraint is that the session id shall be the same as ours)
		std::string sess_id = api_config::get_instance()->session_id();
		std::vector<stream_info_impl> tmp = resolver.resolve_oneshot((std::string("session_id='") += sess_id) += "'",0,wait_time);
		// allocate new stream_info_impl's and assign to the buffer
		uint32_t result = buffer_elements<tmp.size() ? buffer_elements : (uint32_t)tmp.size();
		for (uint32_t k = 0; k < result; k++) buffer[k] = new stream_info_impl(tmp[k]);
		return result;
	} catch(std::exception &e) {
		LOG_F(WARNING, "Error during resolve_all: %s", e.what());
		return lsl_internal_error;
	}
}

/**
* Resolve all streams with a given value for a property.
* If the goal is to resolve a specific stream, this method is preferred over resolving all streams and then selecting the desired one.
* @param buffer A user-allocated buffer to hold the resolve results.
*				Note: it is the user's responsibility to either destroy the resulting streaminfo
*				objects or to pass them back to the LSL during during creation of an inlet.
* @param buffer_elements The user-provided buffer length.
* @param prop The streaminfo property that should have a specific value (e.g., "name", "type", "source_id", or "desc/manufaturer").
* @param value The string value that the property should have (e.g., "EEG" as the type property).
* @param minimum Return at least this number of streams.
* @param timeout Optionally a timeout of the operation, in seconds (default: no timeout).
*				  If the timeout expires, less than the desired number of streams (possibly none) will be returned.
* @return The number of results written into the buffer (never more than the provided # of slots) or a negative number if an
*		  error has occurred (values corresponding to lsl_error_code_t).
*/
LIBLSL_C_API int32_t lsl_resolve_byprop(lsl_streaminfo *buffer, uint32_t buffer_elements, const char *prop, const char *value, int32_t minimum, double timeout) {
	try {
		std::string query{resolver_impl::build_query(prop, value)};
		auto tmp = resolver_impl().resolve_oneshot(query, minimum, timeout);
		// allocate new stream_info_impl's and assign to the buffer
		uint32_t result = buffer_elements<tmp.size() ? buffer_elements : (uint32_t)tmp.size();
		for (uint32_t k = 0; k < result; k++) buffer[k] = new stream_info_impl(tmp[k]);
		return result;
	} catch(std::exception &e) {
		LOG_F(WARNING, "Error during resolve_byprop: %s", e.what());
		return lsl_internal_error;
	}
}

/**
* Resolve all streams that match a given predicate.
* Advanced query that allows to impose more conditions on the retrieved streams; the given string is an XPath 1.0
* predicate for the <info> node (omitting the surrounding []'s), see also http://en.wikipedia.org/w/index.php?title=XPath_1.0&oldid=474981951.
* @param buffer A user-allocated buffer to hold the resolve results.
*				Note: it is the user's responsibility to either destroy the resulting streaminfo
*				objects or to pass them back to the LSL during during creation of an inlet.
* @param buffer_elements The user-provided buffer length.
* @param pred The predicate string, e.g. "name='BioSemi'" or "type='EEG' and starts-with(name,'BioSemi') and count(info/desc/channel)=32"
* @param minimum Return at least this number of streams.
* @param timeout Optionally a timeout of the operation, in seconds (default: no timeout).
*				  If the timeout expires, less than the desired number of streams (possibly none) will be returned.
* @return The number of results written into the buffer (never more than the provided # of slots) or a negative number if an
*		  error has occurred (values corresponding to lsl_error_code_t).
*/
LIBLSL_C_API int32_t lsl_resolve_bypred(lsl_streaminfo *buffer, uint32_t buffer_elements, const char *pred, int32_t minimum, double timeout) {
	try {
		std::string query{resolver_impl::build_query(pred)};
		auto tmp = resolver_impl().resolve_oneshot(query, minimum, timeout);
		// allocate new stream_info_impl's and assign to the buffer
		uint32_t result = buffer_elements<tmp.size() ? buffer_elements : (uint32_t)tmp.size();
		for (uint32_t k = 0; k < result; k++) buffer[k] = new stream_info_impl(tmp[k]);
		return result;
	} catch(std::exception &e) {
		LOG_F(WARNING, "Error during resolve_bypred: %s", e.what());
		return lsl_internal_error;
	}
}
}
