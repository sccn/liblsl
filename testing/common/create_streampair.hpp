#pragma once
#include <lsl_cpp.h>

struct Streampair {
	lsl::stream_outlet out_;
	lsl::stream_inlet in_;

	Streampair(lsl::stream_outlet &&out, lsl::stream_inlet &&in)
		: out_(std::move(out)), in_(std::move(in)) {}
};

inline Streampair create_streampair(const lsl::stream_info &info) {
	lsl::stream_outlet out(info);
	auto found_stream_info(lsl::resolve_stream("name", info.name(), 1, 2.0));
	if (found_stream_info.empty()) throw std::runtime_error("outlet not found");
	lsl::stream_inlet in(found_stream_info[0]);

	in.open_stream(2);
	out.wait_for_consumers(2);
	return Streampair(std::move(out), std::move(in));
}
