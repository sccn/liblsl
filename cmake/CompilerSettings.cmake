# Compiler and language settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
# generate a compilation database (compile_commands.json) for clang tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LSL_WINVER "0x0601" CACHE STRING
        "Windows version (_WIN32_WINNT) to target (defaults to 0x0601 for Windows 7)")

# Configure RPATH for installed executables (must be set before targets are created)
# so they can find liblsl at runtime, e.g. bin/../lib64 on Fedora or bin/../Frameworks.
# Package managers (Homebrew, conda, ...) pass their own CMAKE_INSTALL_RPATH, which must
# not be overridden.
if(NOT CMAKE_INSTALL_RPATH AND UNIX AND NOT ANDROID)
    if(APPLE AND LSL_FRAMEWORK)
        set(_lsl_libdir "${CMAKE_INSTALL_FRAMEWORK_DIR}")
    else()
        set(_lsl_libdir "${CMAKE_INSTALL_LIBDIR}")
    endif()
    if(IS_ABSOLUTE "${_lsl_libdir}")
        set(CMAKE_INSTALL_RPATH "${_lsl_libdir}")
    else()
        file(RELATIVE_PATH _lsl_libdir "/${CMAKE_INSTALL_BINDIR}" "/${_lsl_libdir}")
        if(APPLE)
            set(CMAKE_INSTALL_RPATH "@loader_path/${_lsl_libdir}")
        else()
            set(CMAKE_INSTALL_RPATH "$ORIGIN/${_lsl_libdir}")
        endif()
    endif()
    unset(_lsl_libdir)
endif()
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Determine library type
if(LSL_BUILD_STATIC)
    set(LSL_LIB_TYPE STATIC)
else()
    set(LSL_LIB_TYPE SHARED)
endif()

# Enable position independent code for shared libraries
if(NOT LSL_BUILD_STATIC OR UNIX)
    # shared libraries require relocatable symbols so we enable them by default
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Enable optimizations if requested.
# Not for static libraries: their objects would hold compiler-specific LTO bytecode
# (GCC -fno-fat-lto-objects, MSVC /GL) that other compilers or versions cannot link.
if(LSL_OPTIMIZATIONS AND NOT LSL_BUILD_STATIC)
    # enable LTO (https://en.wikipedia.org/wiki/Interprocedural_optimization
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# Platform-specific settings
if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
if(MSVC)
    # Treat source files as UTF-8 regardless of the system code page (otherwise non-ASCII
    # characters in comments emit C4819 and in string literals break compilation, see #293)
    add_compile_options(/utf-8)
endif()
