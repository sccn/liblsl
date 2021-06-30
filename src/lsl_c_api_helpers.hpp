#pragma once

#include "common.h"
#include <cstdint>
#include <cstring>

/// Helper for LSL_STORE_EXCEPTION_IN
#define LSLCATCHANDSTORE(ecvar, Exception, code)                                                   \
	catch (Exception & e) {                                                                        \
		strncpy(const_cast<char *>(lsl_last_error()), e.what(), LAST_ERROR_SIZE - 1);              \
		int32_t *__ec = ecvar;                                                                     \
		if (__ec != nullptr) *__ec = code;                                                         \
	}

/// Helper for LSL_STORE_EXCEPTION_IN
#define LSLCATCHANDRETURN(Exception, code)                                                         \
	catch (Exception & e) {                                                                        \
		strncpy(const_cast<char *>(lsl_last_error()), e.what(), LAST_ERROR_SIZE - 1);              \
		return code;                                                                               \
	}


/**
 * C API exception helper.
 *
 * Inserts `catch()` statements for stl and liblsl exceptions and sets the
 * variable pointed to by `ec` to the corresponding value from
 * `lsl_error_code_t` and copies the error message to `last_error`.
 *
 * If you're trying to create a function `lsl_foobar_do_bar` that wraps the `FooBar`'s member
 * function `do_bar`, this allows you to write
 *
 * ```
 * function lsl_foobar_do_bar(lsl_foobar obj, int x, double y, int32_t *ec) {
 *	try {
 *		obj->do_bar(x, y);
 *	} LSL_STORE_EXCEPTION_IN(ec)
 * }
 * ```
 *
 * instead of
 *
 * ```
 * function lsl_foobar_do_bar(lsl_foobar obj, int x, double y, int32_t *ec) {
 *	if(ec) *ec = lsl_no_error;
 *	try {
 *		return obj->do_bar(x, y);
 *	}
 *	catch(Ex1 &e) {
 *		if(ec) *ec = lsl_error1;
 *		strncpy(last_error, e.what(), sizeof(last_error-1);
 *	}
 *	catch(Ex2 &) {
 *		if(ec) *ec = lsl_error2;
 * 		strncpy(last_error, e.what(), sizeof(last_error-1);
 *	}
 *	// etc.
 *	return 0;
 * }
 * ```
 */
#define LSL_STORE_EXCEPTION_IN(ecvar)                                                              \
	LSLCATCHANDSTORE(ecvar, lsl::timeout_error, lsl_timeout_error)                                 \
	LSLCATCHANDSTORE(ecvar, lsl::lost_error, lsl_lost_error)                                       \
	LSLCATCHANDSTORE(ecvar, std::range_error, lsl_argument_error)                                  \
	LSLCATCHANDSTORE(ecvar, std::invalid_argument, lsl_argument_error)                             \
	LSLCATCHANDSTORE(ecvar, std::exception, lsl_internal_error)

#define LSL_STORE_EXCEPTION LSL_STORE_EXCEPTION_IN(nullptr)

/// Catch possible exceptions and return an appropriate error code or `lsl_no_error`
#define LSL_RETURN_CAUGHT_EC                                                                       \
	LSLCATCHANDRETURN(lsl::timeout_error, lsl_timeout_error)                                       \
	LSLCATCHANDRETURN(lsl::lost_error, lsl_lost_error)                                             \
	LSLCATCHANDRETURN(std::range_error, lsl_argument_error)                                        \
	LSLCATCHANDRETURN(std::invalid_argument, lsl_argument_error)                                   \
	LSLCATCHANDRETURN(std::exception, lsl_internal_error)                                          \
	return lsl_no_error

/// Try to create a new T object and return a pointer to it or `nullptr` if an exception occured.
template <class Type, typename... T> Type *create_object_noexcept(T &&...args) noexcept {
	try {
		return new Type(std::forward<T>(args)...);
	}
	LSL_STORE_EXCEPTION_IN(nullptr)
	return nullptr;
}
