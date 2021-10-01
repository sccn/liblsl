#include "api_config.h"

#include <catch2/catch.hpp>

namespace lsl {
void test_api_config() {
	lsl::api_config cfg(true);

	cfg.set_option("tuning", "ReceiveSocketBufferSize", "100");
	REQUIRE(cfg.socket_receive_buffer_size_ == 100);
}
}

TEST_CASE("api_config", "[basic]") {
	lsl::test_api_config();
}
