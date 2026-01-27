# =============================================================================
# LSLCMake.cmake - Common CMake utilities for LSL applications
# =============================================================================
# This module provides helper functions for LSL applications.
#
# Functions:
#   LSL_get_target_arch()       - Detect target architecture for package naming
#   LSL_get_os_name()           - Detect OS name for package naming
#   LSL_configure_rpath()       - Configure RPATH for all platforms
#   LSL_install_liblsl()        - Install liblsl with the application
#   LSL_install_mingw_runtime() - Install MinGW runtime DLLs (Windows only)
#   LSL_deploy_qt()             - Deploy Qt libraries with the application
#   LSL_codesign()              - Sign macOS app bundles with entitlements
#
# Usage:
#   find_package(LSL REQUIRED)
#   include(LSLCMake)  # Now available after finding LSL
#
#   LSL_configure_rpath()  # Call before creating targets
#   # ... create your targets ...
#   LSL_install_liblsl(DESTINATION "." FRAMEWORK_DESTINATION "MyApp.app/Contents/Frameworks")
#   LSL_install_mingw_runtime(DESTINATION ".")
# =============================================================================

cmake_minimum_required(VERSION 3.28)

message(STATUS "Included LSLCMake helpers, rev. 18")

# =============================================================================
# LSL_get_target_arch()
# =============================================================================
# Detects the target architecture by compiling a small test file.
# Works correctly even when cross-compiling.
#
# Sets: LSL_ARCH (cached) - one of: arm64, arm, i386, amd64, ia64, ppc64, powerpc, unknown
#
# Example:
#   LSL_get_target_arch()
#   message(STATUS "Building for ${LSL_ARCH}")
# =============================================================================
function(LSL_get_target_arch)
    if(LSL_ARCH)
        return()
    endif()
    file(WRITE "${CMAKE_BINARY_DIR}/arch.c" "
#if defined(__ARM_ARCH_ISA_A64) || defined(__aarch64__)
#error cmake_ARCH arm64
#elif defined(__arm__) || defined(__TARGET_ARCH_ARM)
#error cmake_ARCH arm
#elif defined(__i386) || defined(__i386__) || defined(_M_IX86)
#error cmake_ARCH i386
#elif defined(__x86_64) || defined(__x86_64__) || defined(__amd64) || defined(_M_X64)
#error cmake_ARCH amd64
#elif defined(__ia64) || defined(__ia64__) || defined(_M_IA64)
#error cmake_ARCH ia64
#elif defined(__ppc__) || defined(__ppc) || defined(__powerpc__) \\
  || defined(_ARCH_COM) || defined(_ARCH_PWR) || defined(_ARCH_PPC)  \\
  || defined(_M_MPPC) || defined(_M_PPC)
#if defined(__ppc64__) || defined(__powerpc64__) || defined(__64BIT__)
#error cmake_ARCH ppc64
#else
#error cmake_ARCH powerpc
#endif
#else
#error cmake_ARCH unknown
#endif")
    enable_language(C)
    try_compile(dummy_result "${CMAKE_BINARY_DIR}"
        SOURCES "${CMAKE_BINARY_DIR}/arch.c"
        OUTPUT_VARIABLE ARCH)
    string(REGEX REPLACE ".*cmake_ARCH ([a-z0-9]+).*" "\\1" ARCH "${ARCH}")
    message(STATUS "LSL: Detected architecture: ${ARCH}")
    set(LSL_ARCH "${ARCH}" CACHE INTERNAL "Target architecture")
endfunction()

# =============================================================================
# LSL_get_os_name()
# =============================================================================
# Detects the OS name for package naming.
# On Linux, attempts to get the distribution codename (e.g., "jammy", "noble").
#
# Sets: LSL_OS (cached) - e.g., "macOS", "Win", "jammy", "Linux"
#
# Example:
#   LSL_get_os_name()
#   set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${LSL_OS}_${LSL_ARCH}")
# =============================================================================
function(LSL_get_os_name)
    if(LSL_OS)
        return()
    endif()
    if(APPLE)
        set(_os "macOS")
    elseif(WIN32)
        set(_os "Win")
    else()
        # Try to get Linux distribution codename
        find_program(LSB_RELEASE lsb_release)
        if(LSB_RELEASE)
            execute_process(
                COMMAND ${LSB_RELEASE} -cs
                OUTPUT_VARIABLE _codename
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(_codename AND NOT _codename STREQUAL "n/a")
                set(_os "${_codename}")
            else()
                set(_os "Linux")
            endif()
        else()
            set(_os "Linux")
        endif()
    endif()
    message(STATUS "LSL: Detected OS: ${_os}")
    set(LSL_OS "${_os}" CACHE INTERNAL "Target OS name")
endfunction()

# =============================================================================
# LSL_configure_rpath()
# =============================================================================
# Configures RPATH for the current project. Must be called BEFORE creating targets.
#
# On macOS: Sets @executable_path-relative paths for both app bundles and CLI tools
# On Linux: Sets $ORIGIN-relative paths
# On Windows: No-op (Windows uses PATH / same-directory lookup)
#
# Example:
#   LSL_configure_rpath()
#   add_executable(MyApp main.cpp)
# =============================================================================
function(LSL_configure_rpath)
    if(APPLE)
        # Support both:
        # - App bundles: @executable_path/../Frameworks
        # - CLI tools: @executable_path/Frameworks or @executable_path
        set(CMAKE_INSTALL_RPATH
            "@executable_path/../Frameworks"
            "@executable_path/Frameworks"
            "@executable_path"
            PARENT_SCOPE
        )
    elseif(UNIX AND NOT ANDROID)
        set(CMAKE_INSTALL_RPATH "$ORIGIN;$ORIGIN/../lib" PARENT_SCOPE)
    endif()
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE PARENT_SCOPE)
endfunction()

# =============================================================================
# LSL_install_liblsl()
# =============================================================================
# Installs liblsl alongside the application, handling platform differences:
# - macOS: Copies lsl.framework to specified destination
# - Windows: Copies lsl.dll
# - Linux: Copies liblsl.so with proper symlinks
#
# Automatically detects whether liblsl was found via find_package (imported target)
# or built via FetchContent (regular target) and handles each case appropriately.
#
# Arguments:
#   DESTINATION          - Install destination for DLL/so (required for Windows/Linux)
#   FRAMEWORK_DESTINATION - Install destination for framework (required for macOS GUI apps)
#
# Example (GUI app):
#   LSL_install_liblsl(
#       DESTINATION "."
#       FRAMEWORK_DESTINATION "${PROJECT_NAME}.app/Contents/Frameworks"
#   )
#
# Example (CLI app or Linux/Windows):
#   LSL_install_liblsl(DESTINATION "${CMAKE_INSTALL_LIBDIR}")
# =============================================================================
function(LSL_install_liblsl)
    cmake_parse_arguments(ARG "" "DESTINATION;FRAMEWORK_DESTINATION" "" ${ARGN})

    # Detect if liblsl is from FetchContent (regular target) or find_package (imported)
    set(_lsl_is_fetched FALSE)
    if(TARGET lsl)
        get_target_property(_lsl_imported lsl IMPORTED)
        if(NOT _lsl_imported)
            set(_lsl_is_fetched TRUE)
        endif()
    endif()

    if(APPLE)
        if(NOT ARG_FRAMEWORK_DESTINATION AND NOT ARG_DESTINATION)
            message(FATAL_ERROR "LSL_install_liblsl: FRAMEWORK_DESTINATION or DESTINATION required on macOS")
        endif()
        # Determine destination - prefer FRAMEWORK_DESTINATION for GUI apps
        set(_fw_dest "${ARG_FRAMEWORK_DESTINATION}")
        if(NOT _fw_dest)
            set(_fw_dest "${ARG_DESTINATION}")
        endif()
        # Use install(CODE) with generator expressions for FetchContent compatibility
        install(CODE "
            set(_lsl_binary \"$<TARGET_FILE:LSL::lsl>\")
            cmake_path(GET _lsl_binary PARENT_PATH _lsl_fw_dir)  # Versions/A
            cmake_path(GET _lsl_fw_dir PARENT_PATH _lsl_fw_dir)  # Versions
            cmake_path(GET _lsl_fw_dir PARENT_PATH _lsl_fw_dir)  # lsl.framework
            message(STATUS \"LSL: Bundling lsl.framework from: \${_lsl_fw_dir}\")
            file(COPY \"\${_lsl_fw_dir}\"
                DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${_fw_dest}\"
                USE_SOURCE_PERMISSIONS
            )
        ")
    elseif(WIN32)
        if(NOT ARG_DESTINATION)
            message(FATAL_ERROR "LSL_install_liblsl: DESTINATION required on Windows")
        endif()
        if(_lsl_is_fetched)
            install(TARGETS lsl RUNTIME DESTINATION "${ARG_DESTINATION}")
        else()
            install(IMPORTED_RUNTIME_ARTIFACTS LSL::lsl RUNTIME DESTINATION "${ARG_DESTINATION}")
        endif()
    else()
        # Linux
        if(NOT ARG_DESTINATION)
            message(FATAL_ERROR "LSL_install_liblsl: DESTINATION required on Linux")
        endif()
        if(_lsl_is_fetched)
            install(TARGETS lsl LIBRARY DESTINATION "${ARG_DESTINATION}")
        else()
            install(IMPORTED_RUNTIME_ARTIFACTS LSL::lsl LIBRARY DESTINATION "${ARG_DESTINATION}")
        endif()
    endif()
endfunction()

# =============================================================================
# LSL_install_mingw_runtime()
# =============================================================================
# Installs MinGW runtime DLLs so executables work outside the build environment.
# Only has effect when building with MinGW; no-op on other compilers.
#
# Arguments:
#   DESTINATION - Install destination for DLLs (required)
#
# Example:
#   LSL_install_mingw_runtime(DESTINATION ".")
# =============================================================================
function(LSL_install_mingw_runtime)
    if(NOT MINGW)
        return()
    endif()

    cmake_parse_arguments(ARG "" "DESTINATION" "" ${ARGN})

    if(NOT ARG_DESTINATION)
        message(FATAL_ERROR "LSL_install_mingw_runtime: DESTINATION required")
    endif()

    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(MINGW_RUNTIME_DLLS
        "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
        "${MINGW_BIN_DIR}/libstdc++-6.dll"
        "${MINGW_BIN_DIR}/libwinpthread-1.dll"
    )
    foreach(_dll ${MINGW_RUNTIME_DLLS})
        if(EXISTS "${_dll}")
            install(FILES "${_dll}" DESTINATION "${ARG_DESTINATION}")
        endif()
    endforeach()
endfunction()

# =============================================================================
# LSL_deploy_qt()
# =============================================================================
# Deploys Qt libraries alongside the application using platform-specific tools.
# - Windows: Uses windeployqt to copy Qt DLLs and plugins
# - macOS: Uses macdeployqt to bundle Qt frameworks
# - Linux: No-op (Qt libraries typically come from system packages)
#
# Requires Qt6::qmake target to be available (from find_package(Qt6)).
#
# Arguments:
#   TARGET      - Target name (without .exe or .app extension)
#   DESTINATION - Install destination directory
#
# Example:
#   LSL_deploy_qt(TARGET "${PROJECT_NAME}" DESTINATION ".")
# =============================================================================
function(LSL_deploy_qt)
    cmake_parse_arguments(ARG "" "TARGET;DESTINATION" "" ${ARGN})

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "LSL_deploy_qt: TARGET required")
    endif()
    if(NOT ARG_DESTINATION)
        message(FATAL_ERROR "LSL_deploy_qt: DESTINATION required")
    endif()

    if(NOT TARGET Qt6::qmake)
        message(WARNING "LSL_deploy_qt: Qt6::qmake not found, skipping Qt deployment")
        return()
    endif()

    get_target_property(_qt_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake_executable}" DIRECTORY)

    if(WIN32)
        find_program(_windeployqt_executable windeployqt HINTS "${_qt_bin_dir}")
        if(_windeployqt_executable)
            install(CODE "
                message(STATUS \"Running windeployqt...\")
                execute_process(
                    COMMAND \"${_windeployqt_executable}\"
                        --no-translations
                        --no-system-d3d-compiler
                        --no-opengl-sw
                        --no-compiler-runtime
                        --dir \"\${CMAKE_INSTALL_PREFIX}/${ARG_DESTINATION}\"
                        \"\${CMAKE_INSTALL_PREFIX}/${ARG_DESTINATION}/${ARG_TARGET}.exe\"
                )
            ")
        else()
            message(WARNING "LSL_deploy_qt: windeployqt not found")
        endif()
    elseif(APPLE)
        find_program(_macdeployqt_executable macdeployqt HINTS "${_qt_bin_dir}")
        if(_macdeployqt_executable)
            install(CODE "
                message(STATUS \"Running macdeployqt...\")
                execute_process(
                    COMMAND \"${_macdeployqt_executable}\"
                        \"\${CMAKE_INSTALL_PREFIX}/${ARG_DESTINATION}/${ARG_TARGET}.app\"
                        -verbose=0
                        -always-overwrite
                    RESULT_VARIABLE _deploy_result
                    ERROR_QUIET
                )
                if(NOT _deploy_result EQUAL 0)
                    message(WARNING \"macdeployqt returned \${_deploy_result}\")
                endif()
            ")
        else()
            message(WARNING "LSL_deploy_qt: macdeployqt not found")
        endif()
    endif()
    # Linux: No-op - Qt libraries typically installed via system packages
endfunction()

# =============================================================================
# LSL_codesign()
# =============================================================================
# Signs macOS app bundles or executables with entitlements for ad-hoc local development.
# Uses the "-" identity (ad-hoc signing) which doesn't require a Developer ID certificate.
#
# For release builds with proper signing/notarization, use a separate CI script
# with an actual Developer ID certificate.
#
# Arguments:
#   TARGET       - Target name (without .app extension for bundles)
#   DESTINATION  - Install destination directory
#   ENTITLEMENTS - Path to entitlements file (required)
#   BUNDLE       - If set, signs as app bundle (.app), otherwise signs as executable
#   FRAMEWORK    - Optional: Path to framework to sign before the executable (for CLI apps)
#
# Example (GUI app bundle):
#   LSL_codesign(
#       TARGET "${PROJECT_NAME}"
#       DESTINATION "${INSTALL_BINDIR}"
#       ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/app.entitlements"
#       BUNDLE
#   )
#
# Example (CLI executable with framework):
#   LSL_codesign(
#       TARGET "${PROJECT_NAME}CLI"
#       DESTINATION "${INSTALL_BINDIR}"
#       ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/app.entitlements"
#       FRAMEWORK "Frameworks/lsl.framework"
#   )
# =============================================================================
function(LSL_codesign)
    if(NOT APPLE)
        return()
    endif()

    cmake_parse_arguments(ARG "BUNDLE" "TARGET;DESTINATION;ENTITLEMENTS;FRAMEWORK" "" ${ARGN})

    if(NOT ARG_TARGET)
        message(FATAL_ERROR "LSL_codesign: TARGET required")
    endif()
    if(NOT ARG_DESTINATION)
        message(FATAL_ERROR "LSL_codesign: DESTINATION required")
    endif()
    if(NOT ARG_ENTITLEMENTS)
        message(FATAL_ERROR "LSL_codesign: ENTITLEMENTS required")
    endif()

    if(ARG_BUNDLE)
        # Sign app bundle
        install(CODE "
            set(_app \"\${CMAKE_INSTALL_PREFIX}/${ARG_DESTINATION}/${ARG_TARGET}.app\")
            set(_ent \"${ARG_ENTITLEMENTS}\")

            message(STATUS \"Signing app bundle...\")
            execute_process(
                COMMAND codesign --force --deep --sign - --entitlements \"\${_ent}\" \"\${_app}\"
                RESULT_VARIABLE _sign_result
            )

            execute_process(COMMAND codesign --verify --verbose \"\${_app}\" RESULT_VARIABLE _verify_result)
            if(_verify_result EQUAL 0)
                message(STATUS \"App bundle signature verified successfully\")
            else()
                message(WARNING \"App bundle signature verification failed!\")
            endif()
        ")
    else()
        # Sign executable (and optionally framework first)
        install(CODE "
            set(_exe \"\${CMAKE_INSTALL_PREFIX}/${ARG_DESTINATION}/${ARG_TARGET}\")
            set(_ent \"${ARG_ENTITLEMENTS}\")

            # Sign framework first if specified
            if(NOT \"${ARG_FRAMEWORK}\" STREQUAL \"\")
                set(_fw \"\${CMAKE_INSTALL_PREFIX}/${ARG_FRAMEWORK}\")
                message(STATUS \"Signing framework: \${_fw}\")
                execute_process(COMMAND codesign --force --sign - \"\${_fw}\")
            endif()

            message(STATUS \"Signing executable: \${_exe}\")
            execute_process(
                COMMAND codesign --force --sign - --entitlements \"\${_ent}\" \"\${_exe}\"
                RESULT_VARIABLE _sign_result
            )
        ")
    endif()
endfunction()


# =============================================================================
# DEPRECATED FUNCTIONS
# =============================================================================
# The following functions are deprecated and will be removed in a future version.
# They are kept for backward compatibility with existing liblsl builds.
# New applications should use the modern functions above.
# =============================================================================

# Deprecated: Use standard install() commands instead
function(installLSLApp target)
    message(DEPRECATION "installLSLApp() is deprecated. Use standard CMake install() commands.")

    # Basic install for the target
    include(GNUInstallDirs)
    install(TARGETS ${target}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endfunction()

# Deprecated: Use standard install(FILES/DIRECTORY) instead
function(installLSLAuxFiles target)
    message(DEPRECATION "installLSLAuxFiles() is deprecated. Use standard CMake install() commands.")

    include(GNUInstallDirs)
    set(destdir "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}")

    if("${ARGV1}" STREQUAL "directory")
        install(DIRECTORY ${ARGV2} DESTINATION ${destdir})
    else()
        install(FILES ${ARGN} DESTINATION ${destdir})
    endif()
endfunction()

# Deprecated: Apps should configure CPack themselves
macro(LSLGenerateCPackConfig)
    message(DEPRECATION "LSLGenerateCPackConfig() is deprecated. Configure CPack directly in your CMakeLists.txt.")

    LSL_get_target_arch()
    LSL_get_os_name()

    set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    if(NOT CPACK_PACKAGE_VENDOR)
        set(CPACK_PACKAGE_VENDOR "Labstreaminglayer")
    endif()
    set(CPACK_STRIP_FILES ON)

    if(APPLE)
        set(CPACK_GENERATOR TGZ)
    elseif(WIN32)
        set(CPACK_GENERATOR ZIP)
    else()
        set(CPACK_GENERATOR DEB TGZ)
        if(NOT CPACK_DEBIAN_PACKAGE_MAINTAINER)
            set(CPACK_DEBIAN_PACKAGE_MAINTAINER "LabStreamingLayer Developers")
        endif()
        set(CPACK_DEBIAN_PACKAGE_SECTION "science")
    endif()

    set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${LSL_OS}_${LSL_ARCH}")
    include(CPack)
endmacro()
