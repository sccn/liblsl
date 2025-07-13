find_package(Threads REQUIRED)

# Git version information
include(cmake/GitVersion.cmake)

# PugiXML dependency
if(LSL_BUNDLED_PUGIXML)
    message(STATUS "Using bundled pugixml")
    # We will later add the pugixml source files to the lslobj target
else()
    message(STATUS "Using system pugixml")
    find_package(pugixml REQUIRED)
    if(NOT TARGET pugixml::pugixml)
        add_library(pugixml::pugixml ALIAS pugixml)
    endif()
endif()

# Create lslboost target
add_library(lslboost INTERFACE)
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
