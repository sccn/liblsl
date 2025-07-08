# Create object library so all files are only compiled once
add_library(lslobj OBJECT
        ${lslsources}
        ${lslheaders}
)

# Set the includes/headers for the lslobj target
# Note: We cannot use the PUBLIC_HEADER property of the target, because
#  it flattens the include directories.
# We could use the new FILE_SET feature that comes with CMake 3.23.
# This is how it would look. Note that HEADERS is a special set name and implies its type.
#target_sources(lslobj
#    PUBLIC
#        FILE_SET HEADERS
#        BASE_DIRS include
#        FILES ${lslheaders}
#)
# We settle on the older and more common target_include_directories PUBLIC approach.
# If we used the FILET_SET approach then we would remove the PUBLIC includes below.
target_include_directories(lslobj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:include>
    INTERFACE
        # Propagate include directories to consumers
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src>  # for unit tests
)

# Link system libs
target_link_libraries(lslobj PRIVATE lslboost Threads::Threads)
if(MINGW)
    target_link_libraries(lslobj PRIVATE bcrypt)
endif()
if(UNIX AND NOT APPLE)
    # check that clock_gettime is present in the stdlib, link against librt otherwise
    include(CheckSymbolExists)
    check_symbol_exists(clock_gettime time.h HAS_GETTIME)
    if(NOT HAS_GETTIME)
        target_link_libraries(lslobj PRIVATE rt)
    endif()
    if(LSL_DEBUGLOG)
        target_link_libraries(lslobj PRIVATE dl)
    endif()
elseif(WIN32)
    target_link_libraries(lslobj PRIVATE iphlpapi winmm mswsock ws2_32)
endif()

# Compiler settings
target_compile_definitions(lslobj
    PRIVATE
        LIBLSL_EXPORTS
        LOGURU_DEBUG_LOGGING=$<BOOL:${LSL_DEBUGLOG}>
    PUBLIC
        ASIO_NO_DEPRECATED
        $<$<CXX_COMPILER_ID:MSVC>:LSLNOAUTOLINK>  # don't use #pragma(lib) in CMake builds
)
if(WIN32)
    target_compile_definitions(lslobj
            PRIVATE
            _CRT_SECURE_NO_WARNINGS
            PUBLIC
            _WIN32_WINNT=${LSL_WINVER}
    )
    if(BUILD_SHARED_LIBS)
        #        set_target_properties(lslobj
        #            PROPERTIES
        #                WINDOWS_EXPORT_ALL_SYMBOLS ON
        #        )
    endif(BUILD_SHARED_LIBS)
endif(WIN32)

# Link in 3rd party dependencies
# - loguru and asio header-only
target_include_directories(lslobj
    SYSTEM PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/loguru>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/asio>
)
if(NOT LSL_OPTIMIZATIONS)
    # build one object file for Asio instead of once every time an Asio function is called. See
    # https://think-async.com/Asio/asio-1.18.2/doc/asio/using.html#asio.using.optional_separate_compilation
    target_sources(lslobj PRIVATE thirdparty/asio_objects.cpp)
    target_compile_definitions(lslobj
        PUBLIC
            ASIO_SEPARATE_COMPILATION
            ASIO_DISABLE_VISIBILITY
    )
endif()

# - pugixml
if(LSL_BUNDLED_PUGIXML)
    target_sources(lslobj PRIVATE thirdparty/pugixml/pugixml.cpp)
    target_include_directories(lslobj
        SYSTEM PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/pugixml>
    )
else()
    target_link_libraries(lslobj PUBLIC pugixml::pugixml)
endif()
