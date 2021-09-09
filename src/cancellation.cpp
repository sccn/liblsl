#include "cancellation.h"
#include <exception>
#include <loguru.hpp>

lsl::cancellable_registry::~cancellable_registry() = default;

lsl::cancellable_obj::~cancellable_obj() { unregister_from_all(); }

void lsl::cancellable_obj::unregister_from_all() {
	try {
		for (auto *obj : registered_at_) obj->unregister_cancellable(this);
		registered_at_.clear();
	} catch (std::exception &e) {
		LOG_F(ERROR,
			"Unexpected error trying to unregister a cancellable object from its registry: %s",
			e.what());
	}
}
