#pragma once
#include "common.h"

/**
* Construct a new continuous_resolver that resolves all streams on the network.
* This is analogous to the functionality offered by the free function resolve_streams().
* @param forget_after When a stream is no longer visible on the network (e.g., because it was shut down),
*                     this is the time in seconds after which it is no longer reported by the resolver.
*                     The recommended default value is 5.0.
*/
extern LIBLSL_C_API lsl_continuous_resolver lsl_create_continuous_resolver(double forget_after);

/**
* Construct a new continuous_resolver that resolves all streams with a specific value for a given property.
* This is analogous to the functionality provided by the free function resolve_stream(prop,value).
* @param prop The stream_info property that should have a specific value (e.g., "name", "type", "source_id", or "desc/manufaturer").
* @param value The string value that the property should have (e.g., "EEG" as the type property).
* @param forget_after When a stream is no longer visible on the network (e.g., because it was shut down),
*                     this is the time in seconds after which it is no longer reported by the resolver.
*                     The recommended default value is 5.0.
*/
extern LIBLSL_C_API lsl_continuous_resolver lsl_create_continuous_resolver_byprop(const char *prop, const char *value, double forget_after);

/**
* Construct a new continuous_resolver that resolves all streams that match a given XPath 1.0 predicate.
* This is analogous to the functionality provided by the free function resolve_stream(pred).
* @param pred The predicate string, e.g. "name='BioSemi'" or "type='EEG' and starts-with(name,'BioSemi') and count(info/desc/channel)=32"
* @param forget_after When a stream is no longer visible on the network (e.g., because it was shut down),
*                     this is the time in seconds after which it is no longer reported by the resolver.
*                     The recommended default value is 5.0.
*/
extern LIBLSL_C_API lsl_continuous_resolver lsl_create_continuous_resolver_bypred(const char *pred, double forget_after);

/**
* Obtain the set of currently present streams on the network (i.e. resolve result).
* @param res A continuous resolver (previously created with one of the lsl_create_continuous_resolver functions).
* @param buffer A user-allocated buffer to hold the current resolve results.
*               Note: it is the user's responsibility to either destroy the resulting streaminfo
*               objects or to pass them back to the LSL during during creation of an inlet.
*               Note 2: The stream_info's returned by the resolver are only short versions that do not
*               include the .desc() field (which can be arbitrarily big). To obtain the full
*               stream information you need to call .info() on the inlet after you have created one.
* @param buffer_elements The user-provided buffer length.
* @return The number of results written into the buffer (never more than the provided # of slots)
*         or a negative number if an error has occurred (values corresponding to lsl_error_code_t).
*/
extern LIBLSL_C_API int32_t lsl_resolver_results(lsl_continuous_resolver res, lsl_streaminfo *buffer, uint32_t buffer_elements);

/**
* Destructor for the continuous resolver.
*/
extern LIBLSL_C_API void lsl_destroy_continuous_resolver(lsl_continuous_resolver res);
