# Compiler and language settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
# generate a compilation database (compile_commands.json) for clang tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LSL_WINVER "0x0601" CACHE STRING
        "Windows version (_WIN32_WINNT) to target (defaults to 0x0601 for Windows 7)")

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

# Enable optimizations if requested
if(LSL_OPTIMIZATIONS)
    # enable LTO (https://en.wikipedia.org/wiki/Interprocedural_optimization
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# Platform-specific settings
if(WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
