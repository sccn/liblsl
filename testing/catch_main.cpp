#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

int main(int argc, char *argv[]) {
	Catch::Session session;
#ifdef _WIN32
	session.configData().waitForKeypress = Catch::WaitForKeypress::BeforeExit;
#endif
	session.configData().runOrder = Catch::RunTests::InRandomOrder;
#ifdef LSL_VERSION_INFO
#define BOOST_LIB_VERSION ""
	std::cout << "liblsl version: " << LSL_VERSION_INFO << '\n';
#endif
	int returnCode = session.applyCommandLine(argc, argv);
	if (returnCode != 0) return returnCode;
	return session.run();
}
