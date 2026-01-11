include(CMakeDependentOption)

# Project build options
option(LSL_DEBUGLOG "Enable (lots of) additional debug messages" OFF)
option(LSL_UNIXFOLDERS "Use the unix folder layout for install targets" ON)
option(LSL_BUILD_STATIC "Build LSL as a static library." OFF)
option(LSL_FRAMEWORK "Build LSL as an Apple Framework (Mac only)" ON)
option(LSL_LEGACY_CPP_ABI "Build legacy C++ ABI into lsl-static" OFF)
option(LSL_OPTIMIZATIONS "Enable some more compiler optimizations" ON)
option(LSL_BUNDLED_BOOST "Use the bundled Boost by default" ON)
option(LSL_BUNDLED_PUGIXML "Use the bundled pugixml by default" ON)
cmake_dependent_option(LSL_TOOLS "Build some experimental tools for in-depth tests" OFF "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(LSL_UNITTESTS "Build LSL library unit tests" OFF "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
cmake_dependent_option(LSL_INSTALL "Generate install targets" ON "CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR" OFF)
option(LSL_FORCE_FANCY_LIBNAME "Add library name decorations (32/64/-debug)" OFF)
mark_as_advanced(LSL_FORCE_FANCY_LIBNAME)

# If we install to the system then we want the framework to land in
# `Library/Frameworks`, otherwise (e.g., Homebrew) we want `Frameworks`
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(FRAMEWORK_DIR_DEFAULT Library/Frameworks)
else()
    set(FRAMEWORK_DIR_DEFAULT Frameworks)
endif()
