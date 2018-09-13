#pragma once

#include "common.h"

/**
* Protocol version.
* The major version is protocol_version() / 100;
* The minor version is protocol_version() % 100;
* Clients with different minor versions are protocol-compatible with each other
* while clients with different major versions will refuse to work together.
*/
extern LIBLSL_C_API int32_t lsl_protocol_version();

/**
* Version of the liblsl library.
* The major version is library_version() / 100;
* The minor version is library_version() % 100;
*/
extern LIBLSL_C_API int32_t lsl_library_version();

/**
* Get a string containing library information. The format of the string shouldn't be used
* for anything important except giving a a debugging person a good idea which exact library
* version is used. */
extern LIBLSL_C_API const char* lsl_library_info();

/**
* Obtain a local system time stamp in seconds. The resolution is better than a millisecond.
* This reading can be used to assign time stamps to samples as they are being acquired.
* If the "age" of a sample is known at a particular time (e.g., from USB transmission
* delays), it can be used as an offset to lsl_local_clock() to obtain a better estimate of
* when a sample was actually captured. See lsl_push_sample() for a use case.
*/
extern LIBLSL_C_API double lsl_local_clock();

/**
* Resolve all streams on the network.
* This function returns all currently available streams from any outlet on the network.
* The network is usually the subnet specified at the local router, but may also include
* a multicast group of machines (given that the network supports it), or a list of hostnames.
* These details may optionally be customized by the experimenter in a configuration file
* (see page Network Connectivity in the LSL wiki).
* This is the default mechanism used by the browsing programs and the recording program.
* @param buffer A user-allocated buffer to hold the resolve results.
*               Note: it is the user's responsibility to either destroy the resulting streaminfo
*               objects or to pass them back to the LSL during during creation of an inlet.
*               Note 2: The stream_info's returned by the resolver are only short versions that do not
*               include the .desc() field (which can be arbitrarily big). To obtain the full
*               stream information you need to call .info() on the inlet after you have created one.
* @param buffer_elements The user-provided buffer length.
* @param wait_time The waiting time for the operation, in seconds, to search for streams.
*                  The recommended wait time is 1 second (or 2 for a busy and large recording operation).
*                  Warning: If this is too short (<0.5s) only a subset (or none) of the
*                           outlets that are present on the network may be returned.
* @return The number of results written into the buffer (never more than the provided # of slots)
*         or a negative number if an error has occurred (values corresponding to lsl_error_code_t).
*/
extern LIBLSL_C_API int32_t lsl_resolve_all(lsl_streaminfo *buffer, uint32_t buffer_elements, double wait_time);

/**
* Resolve all streams with a given value for a property.
* If the goal is to resolve a specific stream, this method is preferred over resolving all streams and then selecting the desired one.
* @param buffer A user-allocated buffer to hold the resolve results.
*               Note: it is the user's responsibility to either destroy the resulting streaminfo
*               objects or to pass them back to the LSL during during creation of an inlet.
*               Note 2: The stream_info's returned by the resolver are only short versions that do not
*               include the .desc() field (which can be arbitrarily big). To obtain the full
*               stream information you need to call .info() on the inlet after you have created one.
* @param buffer_elements The user-provided buffer length.
* @param prop The streaminfo property that should have a specific value ("name", "type", "source_id", or, e.g., "desc/manufaturer" if present).
* @param value The string value that the property should have (e.g., "EEG" as the type).
* @param minimum Return at least this number of streams.
* @param timeout Optionally a timeout of the operation, in seconds (default: no timeout).
*                 If the timeout expires, less than the desired number of streams (possibly none) will be returned.
* @return The number of results written into the buffer (never more than the provided # of slots)
*         or a negative number if an error has occurred (values corresponding to lsl_error_code_t).
*/
extern LIBLSL_C_API int32_t lsl_resolve_byprop(lsl_streaminfo *buffer, uint32_t buffer_elements, const char *prop, const char *value, int32_t minimum, double timeout);

/**
* Resolve all streams that match a given predicate.
* Advanced query that allows to impose more conditions on the retrieved streams; the given string is an XPath 1.0
* predicate for the <info> node (omitting the surrounding []'s), see also http://en.wikipedia.org/w/index.php?title=XPath_1.0&oldid=474981951.
* @param buffer A user-allocated buffer to hold the resolve results.
*               Note: it is the user's responsibility to either destroy the resulting streaminfo
*               objects or to pass them back to the LSL during during creation of an inlet.
*               Note 2: The stream_info's returned by the resolver are only short versions that do not
*               include the .desc() field (which can be arbitrarily big). To obtain the full
*               stream information you need to call .info() on the inlet after you have created one.
* @param buffer_elements The user-provided buffer length.
* @param pred The predicate string, e.g. "name='BioSemi'" or "type='EEG' and starts-with(name,'BioSemi') and count(info/desc/channel)=32"
* @param minimum Return at least this number of streams.
* @param timeout Optionally a timeout of the operation, in seconds (default: no timeout).
*                 If the timeout expires, less than the desired number of streams (possibly none) will be returned.
* @return The number of results written into the buffer (never more than the provided # of slots)
*         or a negative number if an error has occurred (values corresponding to lsl_error_code_t).
*/
extern LIBLSL_C_API int32_t lsl_resolve_bypred(lsl_streaminfo *buffer, uint32_t buffer_elements, const char *pred, int32_t minimum, double timeout);

/**
* Deallocate a string that has been transferred to the application.
* Rarely used: the only use case is to deallocate the contents of
* string-valued samples received from LSL in an application where
* no free() method is available (e.g., in some scripting languages).
*/
extern LIBLSL_C_API void lsl_destroy_string(char *s);
