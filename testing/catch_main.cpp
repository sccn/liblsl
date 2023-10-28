#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>

extern "C" {
const char *lsl_library_info() { return LSL_VERSION_INFO; }
}

int main(int argc, char *argv[]) {
	Catch::Session session;
#ifdef _WIN32
	session.configData().waitForKeypress = Catch::WaitForKeypress::BeforeExit;
#endif
	session.configData().runOrder = Catch::TestRunOrder::Randomized;
	int returnCode = session.applyCommandLine(argc, argv);
	if (returnCode != 0) return returnCode;
	return session.run();
}
