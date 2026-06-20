cmake_minimum_required (VERSION 3.23)

# Derive the version from the most recent git tag (e.g. v1.18.0 -> 1.18.0).
# The literal below is the single source of truth for builds without git/tags.
include(cmake/ProjectVersion.cmake)
lsl_version_from_git(LSL_VERSION LSL_VERSION_FULL FALLBACK "1.17.7")

project (liblsl
	VERSION ${LSL_VERSION}
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
# Use the full tag (incl. any pre-release suffix like b1) for package metadata,
# while PROJECT_VERSION stays strictly numeric as CMake requires.
set(CPACK_PACKAGE_VERSION "${LSL_VERSION_FULL}")
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
    if(LSL_ARCH STREQUAL "amd64" OR LSL_ARCH STREQUAL "arm64" OR LSL_ARCH STREQUAL "i386")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${LSL_ARCH}")
    elseif(LSL_ARCH STREQUAL "arm")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "armhf")
    elseif(LSL_ARCH STREQUAL "ppc64")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "ppc64el")
    elseif(LSL_ARCH STREQUAL "powerpc")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "powerpc")
    endif()
endif()

set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${LSL_OS}_${LSL_ARCH}")
include(CPack)
