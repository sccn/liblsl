include(CMakeDependentOption)

# Project build options
# Default LSL_DEBUGLOG to ON for Debug builds (single-config generators only)
string(COMPARE EQUAL "${CMAKE_BUILD_TYPE}" "Debug" _LSL_DEBUGLOG_DEFAULT)
option(LSL_DEBUGLOG "Enable (lots of) additional debug messages" ${_LSL_DEBUGLOG_DEFAULT})
option(LSL_UNIXFOLDERS "Use the unix folder layout for install targets" ON)
# Follow BUILD_SHARED_LIBS (set by package managers and parent projects) when it is defined.
if(DEFINED BUILD_SHARED_LIBS AND NOT BUILD_SHARED_LIBS)
    set(_lsl_build_static_default ON)
else()
    set(_lsl_build_static_default OFF)
endif()
option(LSL_BUILD_STATIC "Build LSL as a static library." ${_lsl_build_static_default})
option(LSL_FRAMEWORK "Build LSL as an Apple framework" ${APPLE})
option(LSL_LEGACY_CPP_ABI "Build legacy C++ ABI into lsl-static" OFF)
option(LSL_OPTIMIZATIONS "Enable some more compiler optimizations" ON)
option(LSL_BUNDLED_BOOST "Use the bundled Boost by default" ON)
option(LSL_FETCH_PUGIXML "Fetch pugixml via FetchContent instead of using system package" ON)
cmake_dependent_option(LSL_TOOLS "Build some experimental tools for in-depth tests" OFF "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(LSL_UNITTESTS "Build LSL library unit tests" OFF "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(LSL_INSTALL "Generate install targets" ON "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
option(LSL_FORCE_FANCY_LIBNAME "Add library name decorations (32/64/-debug)" OFF)
mark_as_advanced(LSL_FORCE_FANCY_LIBNAME)

# Install directories. Set before any target is created: RPATHs and the exported include
# directories are derived from them.
include(GNUInstallDirs)
if(LSL_UNIXFOLDERS)
    # Relative to the install prefix; pass an absolute path (e.g. /Library/Frameworks) to
    # install the framework outside the prefix.
    set(CMAKE_INSTALL_FRAMEWORK_DIR Frameworks CACHE PATH "Install directory for frameworks on macOS")
else()
    set(CMAKE_INSTALL_BINDIR LSL)
    set(CMAKE_INSTALL_LIBDIR LSL)
    set(CMAKE_INSTALL_INCLUDEDIR LSL/include)
    set(CMAKE_INSTALL_FRAMEWORK_DIR LSL/Frameworks)
endif()
