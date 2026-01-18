find_package(Threads REQUIRED)

# Git version information
include(cmake/GitVersion.cmake)

# PugiXML dependency via FetchContent
# Always use static pugixml to avoid symbol visibility issues
# Note: We use features of pugixml >= 1.15. Once that is available
#  on target platforms' package managers, then we can allow using
#  system pugixml.
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
