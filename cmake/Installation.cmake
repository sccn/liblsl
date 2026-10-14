# Skip installation when included as a subproject (controlled by LSL_INSTALL option)
if(NOT LSL_INSTALL)
    return()
endif()

# Configure installation
include(CMakePackageConfigHelpers)

# Paths (the install directories themselves are set in ProjectOptions.cmake)
set(LSL_CONFIG_INSTALL_DIRS ${CMAKE_INSTALL_LIBDIR}/cmake/lsl)
if(APPLE AND LSL_FRAMEWORK)
    # Also keep a copy of the CMake package inside the framework, where it always was.
    if(IOS)
        list(APPEND LSL_CONFIG_INSTALL_DIRS ${CMAKE_INSTALL_FRAMEWORK_DIR}/lsl.framework/CMake)
    else()
        list(APPEND LSL_CONFIG_INSTALL_DIRS ${CMAKE_INSTALL_FRAMEWORK_DIR}/lsl.framework/Resources/CMake)
    endif()
endif()

# Generate a version file for the package.
write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/LSLConfigVersion.cmake"
        VERSION "${liblsl_VERSION_MAJOR}.${liblsl_VERSION_MINOR}.${liblsl_VERSION_PATCH}"
        COMPATIBILITY AnyNewerVersion
)

# Define installation targets
# Note: We only export the final 'lsl' library, not intermediate targets like lslobj.
# Consumers should link to LSL::lsl, not internal object libraries.
set(LSLTargets lsl)

# Install the targets and store configuration information.
install(TARGETS ${LSLTargets}
    EXPORT LSLTargets
    COMPONENT liblsl
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_FRAMEWORK_DIR}
)
# Frameworks carry their headers in the bundle (see TargetLib.cmake).
# PUBLIC_HEADER would flatten the include tree and FILE_SET does not work with frameworks,
# so the headers are listed here.
if(NOT (APPLE AND LSL_FRAMEWORK))
    install(DIRECTORY include/lsl DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT liblsl)
    install(FILES include/lsl_c.h include/lsl_cpp.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT liblsl)
endif()

# Generate LSLConfig.cmake from a template so it can call find_dependency() for
# the transitive dependencies that a static LSL::lsl requires its consumers to
# link against. LSLConfig.cmake includes the exported LSLTargets.cmake.
configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/LSLConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/LSLConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/lsl
)
foreach(_dir IN LISTS LSL_CONFIG_INSTALL_DIRS)
    install(EXPORT LSLTargets
            FILE LSLTargets.cmake
            COMPONENT liblsl
            NAMESPACE "LSL::"
            DESTINATION ${_dir}
    )
    install(
        FILES
            cmake/LSLCMake.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/LSLConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/LSLConfigVersion.cmake
        COMPONENT liblsl
        DESTINATION ${_dir}
    )
endforeach()

# A macOS framework also gets the layout of a plain library: lib/liblsl.dylib (or .a) and
# the headers in include/ link into the framework, so `-llsl`, pkg-config and build systems
# that know nothing about frameworks work too.
# `dir` and `target` are relative to the install prefix (DESTDIR-aware) or absolute.
function(lsl_install_symlink dir name target)
    if(IS_ABSOLUTE "${target}")
        set(_target "${target}")
    elseif(IS_ABSOLUTE "${dir}")
        set(_target "\${CMAKE_INSTALL_PREFIX}/${target}")
    else()
        file(RELATIVE_PATH _target "/${dir}" "/${target}")
    endif()
    if(NOT IS_ABSOLUTE "${dir}")
        set(dir "\${CMAKE_INSTALL_PREFIX}/${dir}")
    endif()
    install(CODE "
        set(_link \"\$ENV{DESTDIR}${dir}/${name}\")
        message(STATUS \"Symlinking: \${_link} -> ${_target}\")
        file(MAKE_DIRECTORY \"\$ENV{DESTDIR}${dir}\")
        file(CREATE_LINK \"${_target}\" \"\${_link}\" SYMBOLIC)
    " COMPONENT liblsl)
endfunction()
if(APPLE AND LSL_FRAMEWORK AND NOT IOS)
    set(_fw "${CMAKE_INSTALL_FRAMEWORK_DIR}/lsl.framework")
    lsl_install_symlink("${CMAKE_INSTALL_LIBDIR}" "liblsl${CMAKE_${LSL_LIB_TYPE}_LIBRARY_SUFFIX}" "${_fw}/lsl")
    foreach(_hdr lsl lsl_c.h lsl_cpp.h)
        lsl_install_symlink("${CMAKE_INSTALL_INCLUDEDIR}" "${_hdr}" "${_fw}/Headers/${_hdr}")
    endforeach()
endif()

# Generate the pkg-config file
if(LSL_UNIXFOLDERS AND NOT IOS)
    set(LSL_PC_CFLAGS "")
    set(LSL_PC_LIBS "")
    set(LSL_PC_REQUIRES_PRIVATE "")
    set(LSL_PC_LIBS_PRIVATE "")
    if(LSL_BUILD_STATIC)
        # What `pkg-config --static` adds for a static liblsl (see lsllinklibs, Dependencies.cmake)
        set(LSL_PC_CFLAGS " -DLIBLSL_STATIC")
        foreach(_lib IN LISTS lsllinklibs)
            if(_lib STREQUAL "Threads::Threads")
                string(APPEND LSL_PC_LIBS_PRIVATE " ${CMAKE_THREAD_LIBS_INIT}")
            elseif(_lib MATCHES "^pugixml")
                set(LSL_PC_REQUIRES_PRIVATE "pugixml")
            else()
                string(APPEND LSL_PC_LIBS_PRIVATE " -l${_lib}")
            endif()
        endforeach()
    elseif(APPLE)
        # liblsl's install name starts with @rpath, so programs need an RPATH to load it.
        if(LSL_FRAMEWORK)
            cmake_path(ABSOLUTE_PATH CMAKE_INSTALL_FRAMEWORK_DIR BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
                OUTPUT_VARIABLE _rpath)
        else()
            set(_rpath "${CMAKE_INSTALL_FULL_LIBDIR}")
        endif()
        set(LSL_PC_LIBS " -Wl,-rpath,${_rpath}")
    endif()
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/lsl.pc.in
        ${CMAKE_CURRENT_BINARY_DIR}/lsl.pc
        @ONLY
    )
    install(
        FILES ${CMAKE_CURRENT_BINARY_DIR}/lsl.pc
        COMPONENT liblsl
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
endif()
