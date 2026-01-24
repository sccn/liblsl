find_package(Threads REQUIRED)

# Git version information
include(cmake/GitVersion.cmake)

# PugiXML dependency
# Note: FetchContent fetches pugixml 1.15 which has string_view support.
# System pugixml may be older, but the code handles both via pugi_str() helper.
if(LSL_FETCH_PUGIXML)
    message(STATUS "Fetching pugixml via FetchContent")
    include(FetchContent)
    set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(PUGIXML_NO_EXCEPTIONS OFF CACHE BOOL "" FORCE)
    set(PUGIXML_INSTALL OFF CACHE BOOL "" FORCE)
    # Force static library even if parent project sets BUILD_SHARED_LIBS
    set(PUGIXML_BUILD_SHARED_AND_STATIC_LIBS OFF CACHE BOOL "" FORCE)
    set(_lsl_saved_build_shared_libs ${BUILD_SHARED_LIBS})
    set(BUILD_SHARED_LIBS OFF)
    FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG v1.15
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(pugixml)
    set(BUILD_SHARED_LIBS ${_lsl_saved_build_shared_libs})
    unset(_lsl_saved_build_shared_libs)
    if(TARGET pugixml AND NOT TARGET pugixml::pugixml)
        add_library(pugixml::pugixml ALIAS pugixml)
    endif()
    # Hide pugixml symbols - apply hidden visibility to the pugixml target
    set_target_properties(pugixml PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    set(LSL_PUGIXML_IS_FETCHED TRUE)
else()
    message(STATUS "Using system pugixml")
    # Warn if building universal binary on Apple - system pugixml is likely single-arch
    if(APPLE AND CMAKE_OSX_ARCHITECTURES)
        list(LENGTH CMAKE_OSX_ARCHITECTURES _lsl_arch_count)
        if(_lsl_arch_count GREATER 1)
            message(WARNING
                "Building universal binary with system pugixml. "
                "Homebrew and most package managers provide single-architecture binaries. "
                "Consider using -DLSL_FETCH_PUGIXML=ON or setting -DCMAKE_OSX_ARCHITECTURES "
                "to a single architecture.")
        endif()
        unset(_lsl_arch_count)
    endif()
    find_package(pugixml REQUIRED)
    if(NOT TARGET pugixml::pugixml)
        add_library(pugixml::pugixml ALIAS pugixml)
    endif()
    set(LSL_PUGIXML_IS_FETCHED FALSE)
endif()

# Create lslboost target
add_library(lslboost INTERFACE)
set_target_properties(lslboost PROPERTIES FOLDER "liblsl")
if(LSL_BUNDLED_BOOST)
    message(STATUS "Using bundled header-only Boost")
    target_include_directories(lslboost
        INTERFACE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lslboost>
    )
else()
    message(STATUS "Using system Boost")
    find_package(Boost REQUIRED)
    # Map `lslboost` namespace, which LSL code base uses, to system `boost` namespace/headers.
    target_compile_definitions(lslboost INTERFACE lslboost=boost)
    target_link_libraries(lslboost INTERFACE Boost::boost Boost::disable_autolinking)
endif()
target_compile_definitions(lslboost INTERFACE BOOST_ALL_NO_LIB)

# Enable LOGURU stack traces if LSL_DEBUGLOG is enabled.
set_source_files_properties("thirdparty/loguru/loguru.cpp"
        PROPERTIES COMPILE_DEFINITIONS LOGURU_STACKTRACES=$<BOOL:${LSL_DEBUGLOG}>)
