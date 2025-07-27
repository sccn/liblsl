# Configure installation
include(CMakePackageConfigHelpers)

# Paths
if(LSL_UNIXFOLDERS)
    include(GNUInstallDirs)
    if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
        set(FRAMEWORK_DIR_DEFAULT Library/Frameworks)
    else()
        set(FRAMEWORK_DIR_DEFAULT Frameworks)
    endif()
    set(CMAKE_INSTALL_FRAMEWORK_DIR ${FRAMEWORK_DIR_DEFAULT} CACHE PATH "Install directory for frameworks on macOS")
else()
    set(CMAKE_INSTALL_BINDIR LSL)
    set(CMAKE_INSTALL_LIBDIR LSL)
    set(CMAKE_INSTALL_INCLUDEDIR LSL/include)
    set(CMAKE_INSTALL_FRAMEWORK_DIR LSL/Frameworks CACHE PATH "Install directory for frameworks on macOS")
endif()

# For Apple frameworks, we need to next the install directories within the framework.
if(APPLE AND LSL_FRAMEWORK)
    # For the includes, this is insufficient. Later we will create more accessible symlinks.
    set(LSL_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_FRAMEWORK_DIR}/lsl.framework/Versions/A/include)
    set(LSL_CONFIG_INSTALL_DIR ${CMAKE_INSTALL_FRAMEWORK_DIR}/lsl.framework/Resources/CMake)
else()
    set(LSL_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
    set(LSL_CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/lsl)
endif()

# Generate a version file for the package.
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/LSLConfigVersion.cmake"
        VERSION "${liblsl_VERSION_MAJOR}.${liblsl_VERSION_MINOR}.${liblsl_VERSION_PATCH}"
        COMPATIBILITY AnyNewerVersion
)

# Define installation targets
set(LSLTargets lsl)

# Install the targets and store configuration information.
install(TARGETS ${LSLTargets}
    EXPORT LSLTargets
    COMPONENT liblsl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_FRAMEWORK_DIR}
    FILE_SET HEADERS DESTINATION ${LSL_INSTALL_INCLUDEDIR}
)

# Generate the LSLConfig.cmake file and mark it for installation
install(EXPORT LSLTargets
        FILE LSLConfig.cmake
        COMPONENT liblsl
        NAMESPACE "LSL::"
        DESTINATION ${LSL_CONFIG_INSTALL_DIR}
)
# A common alternative to installing the exported package config file is to generate it from a template.
#configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/lslConfig.cmake.in
#        ${CMAKE_CURRENT_BINARY_DIR}/LSLConfig.cmake
#        INSTALL_DESTINATION ${LSL_CONFIG_INSTALL_DIR})
# If we use this method, then we need a corresponding install(FILES ...) command to install the generated file.

# Install the version file and the helper CMake script.
install(
    FILES
        cmake/LSLCMake.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/LSLConfigVersion.cmake
    COMPONENT liblsl
    DESTINATION ${LSL_CONFIG_INSTALL_DIR}
)

if(APPLE AND LSL_FRAMEWORK)
    # Create symlinks for the framework. The variables we want to use to identify the symlink locations
    #  are not available at install time. Instead, we create a script during configuration time that will
    #  be run at install time to create the symlinks.
    configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CreateFrameworkSymlinks.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/CreateFrameworkSymlinks.cmake
            @ONLY
    )
    install(SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/CreateFrameworkSymlinks.cmake COMPONENT liblsl)
endif()
