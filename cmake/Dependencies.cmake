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

# Single source of truth for every external (system/third-party) library lsl needs to link
# against. lslobj (which internal tests also link directly) and the real lsl target both
# link this same list, so it only has to be kept up to date in one place. Having lsl itself
# link these directly (not only indirectly via lslobj) matters for static builds: CMake
# records a STATIC library's PRIVATE link dependencies as $<LINK_ONLY:...> entries in its
# exported INTERFACE_LINK_LIBRARIES, so consumers linking the installed LSL::lsl get them
# transitively instead of having to add them by hand. For shared builds these stay private/
# hidden, which is correct since the dependencies are already resolved inside the shared lib.
set(lsllinklibs Threads::Threads)

if(NOT LSL_PUGIXML_IS_FETCHED)
    if(TARGET pugixml::pugixml)
        list(APPEND lsllinklibs pugixml::pugixml)
    elseif(TARGET pugixml)
        # For pugixml versions before 1.11
        list(APPEND lsllinklibs pugixml)
        # Add an alias for testing/CMakeLists.txt
        add_library(pugixml::pugixml ALIAS pugixml)
    else()
        message(FATAL_ERROR "pugixml library target not found!")
    endif()
endif()

if(MINGW)
    list(APPEND lsllinklibs bcrypt)
endif()

if(UNIX AND NOT APPLE)
    # check that clock_gettime is present in the stdlib, link against librt otherwise
    include(CheckSymbolExists)
    check_symbol_exists(clock_gettime time.h HAS_GETTIME)
    if(NOT HAS_GETTIME)
        list(APPEND lsllinklibs rt)
    endif()
    if(LSL_DEBUGLOG)
        list(APPEND lsllinklibs dl)
    endif()
elseif(WIN32)
    list(APPEND lsllinklibs iphlpapi winmm mswsock ws2_32)
endif()
