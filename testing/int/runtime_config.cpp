#include "api_config.h"
#include <catch2/catch_test_macros.hpp>

// Verifies that configuration provided via the runtime-config API is picked up
// on singleton initialization. Because api_config::get_instance() is guarded by
// std::call_once, only the first configuration wins per-process. This test
// therefore lives in its own executable so the singleton starts fresh.

TEST_CASE("runtime config content overrides defaults", "[api_config][runtime_config]") {
	lsl::api_config::set_api_config_content(
		"[lab]\n"
		"SessionID = runtime_config_test\n"
		"[ports]\n"
		"BasePort = 30000\n"
		"[tuning]\n"
		"UseProtocolVersion = 100\n");

	const auto *cfg = lsl::api_config::get_instance();
	REQUIRE(cfg != nullptr);
	CHECK(cfg->session_id() == "runtime_config_test");
	CHECK(cfg->base_port() == 30000);
	CHECK(cfg->use_protocol_version() == 100);
}
