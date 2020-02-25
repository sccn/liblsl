#pragma once

/** @file api_types.hpp API type forwards
 *
 * The C API uses pointers to opaque structs to hide implementation details.
 *
 * This header defines the actual typedefs so the C++ implementation doesn't
 * have to do error-prone type casts between internal and API types.
 */

#define LSL_TYPES

namespace lsl {
class continuous_resolver_impl;
class resolver_impl;
class stream_info_impl;
class stream_inlet_impl;
class stream_outlet_impl;
} // namespace lsl

namespace pugi {
struct xml_node_struct;
}
typedef lsl::resolver_impl *lsl_continuous_resolver;
typedef lsl::stream_info_impl *lsl_streaminfo;
typedef lsl::stream_outlet_impl *lsl_outlet;
typedef lsl::stream_inlet_impl *lsl_inlet;
typedef pugi::xml_node_struct *lsl_xml_ptr;
