#include "cancellation.h"
#include "loguru/loguru.hpp"

lsl::cancellable_registry::~cancellable_registry() {}

lsl::cancellable_obj::~cancellable_obj() { unregister_from_all(); }

void lsl::cancellable_obj::unregister_from_all() {
	try {
		for (std::set<cancellable_registry*>::iterator i=registered_at_.begin(); i != registered_at_.end(); i++)
			(*i)->unregister_cancellable(this);
		registered_at_.clear();
	} catch(std::exception &e) {
		LOG_F(ERROR,
			"Unexpected error trying to unregister a cancellable object from its registry: %s",
			e.what());
	}
}
