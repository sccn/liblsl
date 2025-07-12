# Create main library
#  It contains one source with the version info string because some generators require it.
#  The remaining source code is built in the lslobj target and later linked into this library.
set_source_files_properties("src/buildinfo.cpp"
        PROPERTIES COMPILE_DEFINITIONS
        LSL_LIBRARY_INFO_STR="${LSL_VERSION_INFO}/link:${LSL_LIB_TYPE}"
)
add_library(lsl ${LSL_LIB_TYPE} src/buildinfo.cpp)
add_library(LSL::lsl ALIAS lsl)

# Configure main library

# Some naming metadata for export
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

# Link dependencies. The only dependency is lslobj, which contains the bulk of the library code and linkages.
# Note: We link PRIVATE because lslobj exposes extra symbols that are not part of the public API
#  but are used by the internal tests.
target_link_libraries(lsl PRIVATE lslobj)

# Set the include directories for the lsl target.
# Note: We had to link lslobj as a PRIVATE dependency, therefore we must manually expose the include directories
get_target_property(LSLOBJ_HEADERS lslobj HEADER_SET)
target_sources(lsl
    INTERFACE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES ${LSLOBJ_HEADERS}
)

# Set compile definitions for lsl
target_compile_definitions(lsl
    PUBLIC
        # defines for LSL_CPP_API export header (shared: dllimport/dllexport)
        $<IF:$<BOOL:${LSL_BUILD_STATIC}>,LIBLSL_STATIC,LIBLSL_EXPORTS>
        # don't use #pragma(lib) in MSVC builds. TODO: Maybe this can be inherited from lslobj or removed on lslobj?
        $<$<CXX_COMPILER_ID:MSVC>:LSLNOAUTOLINK>
)
