cmake_minimum_required (VERSION 3.23)
project (liblsl
	VERSION 1.16.2
	LANGUAGES C CXX
	DESCRIPTION "Labstreaminglayer C/C++ library"
	HOMEPAGE_URL "https://github.com/sccn/liblsl"
)

# API version, to be incremented on backwards-incompatible ABI changes
set(LSL_ABI_VERSION 2)

# Include modular configuration files
include(cmake/ProjectOptions.cmake)  #
include(cmake/CompilerSettings.cmake)  #
include(cmake/Dependencies.cmake)  #
include(cmake/SourceFiles.cmake)  #
include(cmake/TargetObjLib.cmake)  #
include(cmake/TargetLib.cmake)  #
include(cmake/Installation.cmake)  #
include(cmake/TargetOther.cmake)

if(LSL_UNITTESTS AND NOT IOS)
	add_subdirectory(testing)
endif()

# Config for packaging
# TODO: Config for packaging for the library itself is likely to diverge from config for packing applications. e.g.,
# -> Optionally install to system directories
# -> Apple Framework -- different installer type; uses entitlements
LSLGenerateCPackConfig()
