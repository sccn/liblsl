# Create main library
#  It contains one source with the version info string because some generators require it.
#  The remaining source code is built in the lslobj target and later linked into this library.
add_library(lsl ${LSL_LIB_TYPE} src/buildinfo.cpp)

# Configure main library

# Set compile definitions for lsl
target_compile_definitions(lsl
    PUBLIC
        # defines for LSL_CPP_API export header (shared: dllimport/dllexport)
        $<IF:$<BOOL:${LSL_BUILD_STATIC}>,LIBLSL_STATIC,LIBLSL_EXPORTS>
        # don't use #pragma(lib) in MSVC builds. TODO: Maybe this can be inherited from lslobj or removed on lslobj?
        $<$<CXX_COMPILER_ID:MSVC>:LSLNOAUTOLINK>
    PRIVATE
        $<$<BOOL:${LSL_DEBUGLOG}>:DEBUGLOG>
)

# Includes. TODO: Can we not inherit these from lslobj?
target_include_directories(lsl
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link dependencies
target_link_libraries(lsl
    PRIVATE
        lslobj  # TODO: If this is public, does that improve inheritance of includes and compile definitions?
        lslboost  # TODO: Shouldn't be needed -- lslobj already links it
        ${PUGIXML_LIBRARIES}
)

set_target_properties(lsl PROPERTIES
        VERSION ${liblsl_VERSION_MAJOR}.${liblsl_VERSION_MINOR}.${liblsl_VERSION_PATCH}
        SOVERSION ${LSL_ABI_VERSION}
)
if(LSL_FORCE_FANCY_LIBNAME)
    math(EXPR lslplatform "8 * ${CMAKE_SIZEOF_VOID_P}")
    set_target_properties(lsl PROPERTIES
            PREFIX ""
            OUTPUT_NAME "liblsl${lslplatform}"
            DEBUG_POSTFIX "-debug"
    )
endif()
