cmake_minimum_required (VERSION 3.23)
project (liblsl
	VERSION 1.17.5
	LANGUAGES C CXX
	DESCRIPTION "Labstreaminglayer C/C++ library"
	HOMEPAGE_URL "https://github.com/sccn/liblsl"
)

# API version, to be incremented on backwards-incompatible ABI changes
set(LSL_ABI_VERSION 2)

# Enable folder organization in IDEs (Visual Studio, Xcode, CLion)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

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
include(cmake/LSLCMake.cmake)
LSL_get_target_arch()
LSL_get_os_name()

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_VENDOR "Labstreaminglayer")
set(CPACK_STRIP_FILES ON)

if(APPLE)
    set(CPACK_GENERATOR TGZ)
elseif(WIN32)
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR DEB TGZ)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "LabStreamingLayer Developers")
    set(CPACK_DEBIAN_PACKAGE_SECTION "science")
endif()

set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${LSL_OS}_${LSL_ARCH}")
include(CPack)
