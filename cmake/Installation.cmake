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
    EXPORT LSLTargets
    COMPONENT liblsl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Generate the LSLConfig.cmake file and mark it for installation
install(EXPORT LSLTargets
        FILE LSLConfig.cmake
        COMPONENT liblsl
        NAMESPACE "LSL::"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LSL
)
# A common alternative to installing the exported package config file is to generate it from a template.
#configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/lslConfig.cmake.in
#        ${CMAKE_CURRENT_BINARY_DIR}/LSLConfig.cmake
#        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lsl)
# If we use this method, then we need a corresponding install(FILES ...) command to install the generated file.

# Install the version file and the helper CMake script.
install(
    FILES
        cmake/LSLCMake.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/LSLConfigVersion.cmake
    COMPONENT liblsl
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LSL
)
