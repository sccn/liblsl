# Configure installation
include(CMakePackageConfigHelpers)

# Paths
if(LSL_UNIXFOLDERS)
    include(GNUInstallDirs)
else()
    set(CMAKE_INSTALL_BINDIR LSL)
    set(CMAKE_INSTALL_LIBDIR LSL)
    set(CMAKE_INSTALL_INCLUDEDIR LSL/include)
endif()

# Generate a version file for the package.
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/LSLConfigVersion.cmake"
        VERSION "${liblsl_VERSION_MAJOR}.${liblsl_VERSION_MINOR}.${liblsl_VERSION_PATCH}"
        COMPATIBILITY AnyNewerVersion
)

# Define installation targets
set(LSLTargets lsl)
if(LSL_BUILD_STATIC)
    list(APPEND LSLTargets lslobj lslboost)
endif()

# Install the targets and store configuration information.
install(TARGETS ${LSLTargets}
    EXPORT LSLTargets  # generates a CMake package config; TODO: Why the same name as the list of targets?
    COMPONENT liblsl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    # If we use CMake 3.23 FILE_SET, replace INCLUDES line with: FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# TODO: What does this do? Why do we need LSLTargets.cmake in the build dir?
export(EXPORT LSLTargets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/LSLTargets.cmake"
        NAMESPACE LSL::
)

# Generate the LSLConfig.cmake file and mark it for installation
install(EXPORT LSLTargets
        FILE LSLTargets.cmake  # TODO: I think we can use this to generate LSLConfig.cmake, no?
        COMPONENT liblsl
        NAMESPACE "LSL::"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LSL
)
# A common alternative to installing the exported package config file is to generate it from a template.
#configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/lslConfig.cmake.in
#        ${CMAKE_CURRENT_BINARY_DIR}/LSLConfig.cmake
#        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lsl)
# If we use this method, then we need a corresponding install(FILES ...) command to install the generated file.

# Copy hardcoded CMake files to the build directory.
# TODO: Why bother with this copy? Is it not enough to install (into cmake/LSL)?
configure_file(cmake/LSLCMake.cmake "${CMAKE_CURRENT_BINARY_DIR}/LSLCMake.cmake" COPYONLY)
# TODO: Why bother with this copy? Is it not enough to install (into cmake/LSL)?
# TODO: Why use hardcoded files? We can generate the LSLConfig.cmake.
configure_file(cmake/LSLConfig.cmake "${CMAKE_CURRENT_BINARY_DIR}/LSLConfig.cmake" COPYONLY)

# Install the public headers.
# TODO: Verify that this is necessary, given that we already installed the INCLUDES above.
# TODO: Verify if it is still necessary to install the headers if we use FILE_SET.
install(DIRECTORY include/
        COMPONENT liblsl
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install the version file and the helper CMake script.
install(
    FILES
        # TODO: Keep this. But why does the configure_file(... COPYONLY) above exist?
        cmake/LSLCMake.cmake
        # TODO: Next line shouldn't be necessary if install(EXPORT...) uses LSLConfig.cmake instead of LSLTargets.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/LSLConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/LSLConfigVersion.cmake
    COMPONENT liblsl
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LSL
)
