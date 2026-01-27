cmake_minimum_required(VERSION 3.23)
project(lslexamples
	LANGUAGES C CXX
	VERSION 0.2.0)

include(GNUInstallDirs)

set(LSL_SEARCH_PATHS
    ${LSL_INSTALL_ROOT}
    "${CMAKE_CURRENT_LIST_DIR}/../install"  # GHA scripts default install directory
    "${CMAKE_CURRENT_LIST_DIR}/../cmake-build-release/install"  # CLion default if using -DCMAKE_INSTALL_PREFIX=install
)
if(APPLE)
    # Also search in a Frameworks subdirectory for each path
    foreach(p IN LISTS LSL_SEARCH_PATHS)
        if(p)
            list(APPEND LSL_SEARCH_PATHS "${p}/Frameworks")
        endif()
    endforeach()
else()
    # Add MSVC-specific paths if not on Apple
    list(APPEND LSL_SEARCH_PATHS
        "${CMAKE_CURRENT_LIST_DIR}/../cmake-build-release-visual-studio/install"  # CLion default if using VS compiler
        "${CMAKE_CURRENT_LIST_DIR}/../out/build/x64-Release/install"  # MSVC default if using -DCMAKE_INSTALL_PREFIX=install
    )
endif()

find_package(LSL REQUIRED
	HINTS ${LSL_SEARCH_PATHS}
	PATH_SUFFIXES share/LSL
)
get_filename_component(LSL_PATH ${LSL_CONFIG} DIRECTORY)
message(STATUS "Found LSL lib in ${LSL_PATH}")

# Include the LSLCMake.cmake file, just for testing.
# This doesn't do much for us now that we don't use installLSLApp() anymore.
include("${LSL_PATH}/LSLCMake.cmake")

# convenience function to add an example file
# this creates a target, links the necessary libraries and
# creates an install target
function(addlslexample name extension)
	add_executable(${name}
		${name}.${extension}
	)
	target_link_libraries(${name} PRIVATE LSL::lsl)
	target_compile_features(${name} PRIVATE cxx_constexpr)

    # Set RPATH properties for macOS and Linux
    set_target_properties(${name} PROPERTIES
            INSTALL_RPATH_USE_LINK_PATH TRUE
            BUILD_WITH_INSTALL_RPATH FALSE
    )

    # One might also want to do the following, to give the option of copying the
    #  LSL library to the same directory or install tree as the executable.
    # However, this is not necessary for the examples, as they are not intended to be relocated.
#    if(APPLE)
#        set_target_properties(${name} PROPERTIES
#                INSTALL_RPATH "@loader_path;@loader_path/../lib;@loader_path/../Frameworks"
#        )
#    elseif(UNIX)
#        set_target_properties(${name} PROPERTIES
#                INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib"
#        )
#    endif()

    install(TARGETS ${name}
        COMPONENT ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endfunction()

find_package(Threads)

addlslexample(GetAllStreams cpp)
addlslexample(GetFullinfo cpp)
addlslexample(GetTimeCorrection cpp)
addlslexample(HandleMetaData cpp)
addlslexample(HandleMetaDataC c)
addlslexample(ReceiveData cpp)
addlslexample(ReceiveDataC c)
addlslexample(ReceiveDataInChunks cpp)
addlslexample(ReceiveDataSimple cpp)
addlslexample(ReceiveStringMarkers cpp)
addlslexample(ReceiveStringMarkersC c)
addlslexample(SendData cpp)
addlslexample(SendDataC c)
addlslexample(SendDataInChunks cpp)
addlslexample(SendDataSimple cpp)
addlslexample(SendMultipleStreams cpp)
addlslexample(SendStringMarkers cpp)
addlslexample(SendStringMarkersC c)
addlslexample(TestSyncWithoutData cpp)

target_link_libraries(TestSyncWithoutData PRIVATE Threads::Threads)

# Windows doesn't have RPATH so we put the dll into the same directory as the executable.
if(WIN32)
	# For one of the targets, copy the lsl.dll into the build directory so we can debug the examples if needed.
	add_custom_command(TARGET HandleMetaData POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:LSL::lsl>"
        $<TARGET_FILE_DIR:HandleMetaData>
        COMMENT "Copying lsl.dll to examples build directory"
    )
	# Install the lsl.dll to the same directory as the executable.
	install(
        CODE "file(INSTALL DESTINATION \"${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\" TYPE SHARED_LIBRARY FILES \"$<TARGET_FILE:LSL::lsl>\")"
        COMPONENT ${PROJECT_NAME}
    )
endif()
