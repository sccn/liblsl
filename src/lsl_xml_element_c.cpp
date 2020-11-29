#include <pugixml.hpp>

extern "C" {
#include "api_types.hpp"
// include api_types before public API header
#include "../include/lsl/xml.h"

using namespace pugi;

LIBLSL_C_API lsl_xml_ptr lsl_first_child(lsl_xml_ptr e) {
	return xml_node(e).first_child().internal_object();
}
LIBLSL_C_API lsl_xml_ptr lsl_last_child(lsl_xml_ptr e) {
	return xml_node(e).last_child().internal_object();
}
LIBLSL_C_API lsl_xml_ptr lsl_next_sibling(lsl_xml_ptr e) {
	return xml_node(e).next_sibling().internal_object();
}
LIBLSL_C_API lsl_xml_ptr lsl_previous_sibling(lsl_xml_ptr e) {
	return xml_node(e).previous_sibling().internal_object();
}
LIBLSL_C_API lsl_xml_ptr lsl_parent(lsl_xml_ptr e) {
	return xml_node(e).parent().internal_object();
}
LIBLSL_C_API lsl_xml_ptr lsl_child(lsl_xml_ptr e, const char *name) {
	return xml_node(e).child(name).internal_object();
}
LIBLSL_C_API lsl_xml_ptr lsl_next_sibling_n(lsl_xml_ptr e, const char *name) {
	return xml_node(e).next_sibling(name).internal_object();
}
LIBLSL_C_API lsl_xml_ptr lsl_previous_sibling_n(lsl_xml_ptr e, const char *name) {
	return xml_node(e).previous_sibling(name).internal_object();
}

LIBLSL_C_API int32_t lsl_empty(lsl_xml_ptr e) { return xml_node(e).empty(); }
LIBLSL_C_API int32_t lsl_is_text(lsl_xml_ptr e) { return xml_node(e).type() != node_element; }
LIBLSL_C_API const char *lsl_name(lsl_xml_ptr e) { return xml_node(e).name(); }
LIBLSL_C_API const char *lsl_value(lsl_xml_ptr e) { return xml_node(e).value(); }
LIBLSL_C_API const char *lsl_child_value(lsl_xml_ptr e) { return xml_node(e).child_value(); }
LIBLSL_C_API const char *lsl_child_value_n(lsl_xml_ptr e, const char *name) {
	return xml_node(e).child_value(name);
}

LIBLSL_C_API int32_t lsl_set_name(lsl_xml_ptr e, const char *rhs) {
	return xml_node(e).set_name(rhs);
}
LIBLSL_C_API int32_t lsl_set_value(lsl_xml_ptr e, const char *rhs) {
	return xml_node(e).set_value(rhs);
}
LIBLSL_C_API lsl_xml_ptr lsl_append_child(lsl_xml_ptr e, const char *name) {
	return xml_node(e).append_child(name).internal_object();
}
LIBLSL_C_API lsl_xml_ptr lsl_prepend_child(lsl_xml_ptr e, const char *name) {
	return xml_node(e).prepend_child(name).internal_object();
}
LIBLSL_C_API lsl_xml_ptr lsl_append_copy(lsl_xml_ptr e, lsl_xml_ptr e2) {
	return xml_node(e).append_copy(xml_node(e2)).internal_object();
}
LIBLSL_C_API lsl_xml_ptr lsl_prepend_copy(lsl_xml_ptr e, lsl_xml_ptr e2) {
	return xml_node(e).prepend_copy(xml_node(e2)).internal_object();
}
LIBLSL_C_API void lsl_remove_child_n(lsl_xml_ptr e, const char *name) {
	xml_node(e).remove_child(name);
}
LIBLSL_C_API void lsl_remove_child(lsl_xml_ptr e, lsl_xml_ptr e2) {
	xml_node(e).remove_child(xml_node(e2));
}

LIBLSL_C_API int32_t lsl_set_child_value(lsl_xml_ptr e, const char *name, const char *value) {
	return xml_node(e).child(name).first_child().set_value(value);
}

LIBLSL_C_API lsl_xml_ptr lsl_append_child_value(
	lsl_xml_ptr e, const char *name, const char *value) {
	xml_node result = xml_node(e).append_child(name);
	result.append_child(node_pcdata).set_value(value);
	return e;
}

LIBLSL_C_API lsl_xml_ptr lsl_prepend_child_value(
	lsl_xml_ptr e, const char *name, const char *value) {
	xml_node result = xml_node(e).prepend_child(name);
	result.append_child(node_pcdata).set_value(value);
	return e;
}
}
