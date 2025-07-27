# Create main library
#  It contains one source with the version info string because some generators require it.
#  The remaining source code is built in the lslobj target and later linked into this library.
set_source_files_properties("src/buildinfo.cpp"
        PROPERTIES COMPILE_DEFINITIONS
        LSL_LIBRARY_INFO_STR="${LSL_VERSION_INFO}/link:${LSL_LIB_TYPE}"
)
add_library(lsl ${LSL_LIB_TYPE} src/buildinfo.cpp)
add_library(LSL::lsl ALIAS lsl)

# Configure main library

# Some naming metadata for export
set_target_properties(lsl PROPERTIES
        VERSION ${liblsl_VERSION_MAJOR}.${liblsl_VERSION_MINOR}.${liblsl_VERSION_PATCH}
        SOVERSION ${LSL_ABI_VERSION}
)
if(LSL_FORCE_FANCY_LIBNAME)
    math(EXPR lslplatform "8 * ${CMAKE_SIZEOF_VOID_P}")
    set_target_properties(lsl PROPERTIES
            PREFIX ""
            OUTPUT_NAME "liblsl${lslplatform}"
            DEBUG_POSTFIX "-debug"
    )
endif()

# Link dependencies. The only dependency is lslobj, which contains the bulk of the library code and linkages.
# Note: We link PRIVATE because lslobj exposes extra symbols that are not part of the public API
#  but are used by the internal tests.
target_link_libraries(lsl PRIVATE lslobj)

# Set the include directories for the lsl target.
# Note: We had to link lslobj as a PRIVATE dependency, therefore we must manually expose the include directories
if(APPLE AND LSL_FRAMEWORK)
    # For frameworks, the install interface needs to point into the framework bundle
    if(LSL_UNIXFOLDERS)
        set(LSL_INSTALL_INTERFACE_INCLUDE_DIR "${FRAMEWORK_DIR_DEFAULT}/lsl.framework/Versions/A/Headers")
    else()
        set(LSL_INSTALL_INTERFACE_INCLUDE_DIR "LSL/Frameworks/lsl.framework/Versions/A/Headers")
    endif()
else()
    set(LSL_INSTALL_INTERFACE_INCLUDE_DIR "include")
endif()
target_include_directories(lsl
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${LSL_INSTALL_INTERFACE_INCLUDE_DIR}>
)

# Set compile definitions for lsl
target_compile_definitions(lsl
    PUBLIC
        # defines for LSL_CPP_API export header (shared: dllimport/dllexport)
        $<IF:$<BOOL:${LSL_BUILD_STATIC}>,LIBLSL_STATIC,LIBLSL_EXPORTS>
        # don't use #pragma(lib) in MSVC builds. TODO: Maybe this can be inherited from lslobj or removed on lslobj?
        $<$<CXX_COMPILER_ID:MSVC>:LSLNOAUTOLINK>
)

# Extra configuration for Apple targets -- set xcode attributes
if(APPLE)
    set_target_properties(lsl PROPERTIES
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.labstreaminglayer.liblsl"
    )
    # If environment variables are set for Apple Development Team and Code Sign Identity then add these to the target
    # -> if `-G Xcode` generator is used then Xcode will use these variables to sign the framework.
    # Note, however, that it is likely that the build products will be modified post-build, invalidating the signature,
    #  so post-hoc signing will be required. Nevertheless, this is useful for initial signing and normal Xcode workflow.
    if(DEFINED ENV{APPLE_DEVELOPMENT_TEAM} AND DEFINED ENV{APPLE_CODE_SIGN_IDENTITY_APP})
        set_target_properties(lsl PROPERTIES
                XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY $ENV{APPLE_CODE_SIGN_IDENTITY_APP}
                XCODE_ATTRIBUTE_DEVELOPMENT_TEAM $ENV{APPLE_DEVELOPMENT_TEAM}
                XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
                XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING YES   # this is needed for strip symbols
                XCODE_ATTRIBUTE_OTHER_CODE_SIGN_FLAGS "--deep"
        )
    endif()
    # Configure Apple Framework
    if(LSL_FRAMEWORK)
        set_target_properties(lsl PROPERTIES
                FRAMEWORK TRUE
                FRAMEWORK_VERSION A  # Ignored on iOS
                MACOSX_FRAMEWORK_IDENTIFIER "org.labstreaminglayer.liblsl"
                MACOSX_FRAMEWORK_SHORT_VERSION_STRING "${liblsl_VERSION_MAJOR}.${liblsl_VERSION_MINOR}"
                MACOSX_FRAMEWORK_BUNDLE_VERSION ${PROJECT_VERSION}
                XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/lsl.entitlements"
        )
    endif(LSL_FRAMEWORK)
endif(APPLE)
