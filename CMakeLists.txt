cmake_minimum_required (VERSION 3.12)
project (liblsl
	VERSION 1.13.1
	LANGUAGES C CXX
	DESCRIPTION "Labstreaminglayer C/C++ library"
	HOMEPAGE_URL "https://github.com/sccn/liblsl"
	)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE On)
if(NOT MSVC_VERSION VERSION_LESS 1700)
	set(CMAKE_CXX_STANDARD 14)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LSL_UNIXFOLDERS "Use the unix folder layout for install targets" On)
option(LSL_NO_FANCY_LIBNAME "Skip library name decorations (32/64/-debug)")
option(LSL_BUILD_EXAMPLES "Build example programs in examples/" OFF)
option(LSL_BUILD_STATIC "Also build static LSL library." OFF)
option(LSL_LEGACY_CPP_ABI "Build legacy C++ ABI into lsl-static" OFF)
option(LSL_UNITTESTS "Build LSL library unit tests" OFF)

set(LSL_WINVER "0x0601" CACHE STRING
	"Windows version (_WIN32_WINNT) to target (defaults to 0x0601 for Windows 7)")

# Add an object library so all files are only compiled once
set(lslobj_sources
	src/api_config.cpp
	src/api_config.h
	src/cancellable_streambuf.h
	src/cancellation.h
	src/cancellation.cpp
	src/cast.h
	src/cast.cpp
	src/common.cpp
	src/common.h
	src/consumer_queue.cpp
	src/consumer_queue.h
	src/data_receiver.cpp
	src/data_receiver.h
	src/forward.h
	src/info_receiver.cpp
	src/info_receiver.h
	src/inlet_connection.cpp
	src/inlet_connection.h
	src/lsl_continuous_resolver_c.cpp
	src/lsl_freefuncs_c.cpp
	src/lsl_inlet_c.cpp
	src/lsl_outlet_c.cpp
	src/lsl_streaminfo_c.cpp
	src/lsl_xml_element_c.cpp
	src/portable_archive/portable_archive_exception.hpp
	src/portable_archive/portable_archive_includes.hpp
	src/portable_archive/portable_iarchive.hpp
	src/portable_archive/portable_oarchive.hpp
	src/pugixml/pugiconfig.hpp
	src/pugixml/pugixml.cpp
	src/pugixml/pugixml.hpp
	src/resolver_impl.cpp
	src/resolver_impl.h
	src/resolve_attempt_udp.cpp
	src/resolve_attempt_udp.h
	src/sample.cpp
	src/sample.h
	src/send_buffer.cpp
	src/send_buffer.h
	src/socket_utils.cpp
	src/socket_utils.h
	src/stream_info_impl.cpp
	src/stream_info_impl.h
	src/stream_inlet_impl.h
	src/stream_outlet_impl.cpp
	src/stream_outlet_impl.h
	src/tcp_server.cpp
	src/tcp_server.h
	src/time_postprocessor.cpp
	src/time_postprocessor.h
	src/time_receiver.cpp
	src/time_receiver.h
	src/udp_server.cpp
	src/udp_server.h
	$<$<BOOL:${LSL_LEGACY_CPP_ABI}>:src/legacy/legacy_abi.cpp src/legacy/legacy_abi.h>
	# headers
	include/lsl_c.h
	include/lsl_cpp.h
	include/lsl/constants.h
)
add_library(lslobj OBJECT ${lslobj_sources})

# try to find out which revision is currently checked out
find_package(Git)
if(lslgitrevision AND lslgitbranch)
	message(STATUS "Got git information ${lslgitrevision}/${lslgitbranch} from the command line")
elseif(GIT_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --tags HEAD
		WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
		OUTPUT_VARIABLE lslgitrevision
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --symbolic-full-name --abbrev-ref @
		WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
		OUTPUT_VARIABLE lslgitbranch
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	message(STATUS "Git version information: ${lslgitbranch}/${lslgitrevision}")
else()
	set(lslgitrevision "unknown")
	set(lslgitbranch "unknown")
endif()
set(LSL_VERSION_INFO "\"git:${lslgitrevision}/branch:${lslgitbranch}/build:${CMAKE_BUILD_TYPE}/compiler:${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}/boost:\" BOOST_LIB_VERSION")
set_source_files_properties("src/lsl_freefuncs_c.cpp" PROPERTIES COMPILE_DEFINITIONS LSL_LIBRARY_INFO_STR=${LSL_VERSION_INFO})


## create the lslboost target
add_library(lslboost OBJECT
	lslboost/asio_objects.cpp
	lslboost/libs/atomic/src/lockpool.cpp
	lslboost/libs/chrono/src/chrono.cpp
	lslboost/libs/serialization/src/archive_exception.cpp
	lslboost/libs/serialization/src/basic_archive.cpp
	lslboost/libs/serialization/src/basic_iarchive.cpp
	lslboost/libs/serialization/src/basic_iserializer.cpp
	lslboost/libs/serialization/src/basic_oarchive.cpp
	lslboost/libs/serialization/src/basic_oserializer.cpp
	lslboost/libs/serialization/src/basic_serializer_map.cpp
	lslboost/libs/serialization/src/extended_type_info.cpp
	lslboost/libs/serialization/src/extended_type_info_typeid.cpp
	lslboost/libs/serialization/src/void_cast.cpp
)
if (UNIX)
	target_sources(lslboost PRIVATE
		lslboost/libs/thread/src/pthread/once.cpp
		lslboost/libs/thread/src/pthread/thread.cpp
	)
	find_package(Threads REQUIRED)
	target_link_libraries(lslboost PUBLIC Threads::Threads)
	target_compile_features(lslboost PUBLIC cxx_std_11)
else ()  # WIN32
	target_sources(lslboost PRIVATE
		lslboost/libs/serialization/src/codecvt_null.cpp
		lslboost/libs/thread/src/win32/thread.cpp
		lslboost/libs/thread/src/win32/tss_dll.cpp
		lslboost/libs/thread/src/win32/tss_pe.cpp
	)
	target_link_libraries(lslboost PRIVATE
		bcrypt
		$<$<BOOL:${MINGW}>:wsock32 ws2_32 winmm>
	)
endif ()

target_compile_definitions(lslboost
	PUBLIC
		BOOST_ALL_NO_LIB
		BOOST_ASIO_SEPARATE_COMPILATION
		$<$<PLATFORM_ID:Windows>:_WIN32_WINNT=${LSL_WINVER}>
	PRIVATE
		$<$<PLATFORM_ID:Windows>:BOOST_THREAD_BUILD_DLL>
)
target_include_directories(lslboost PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lslboost>)


target_link_libraries(lslobj PRIVATE lslboost)
target_include_directories(lslobj
	PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)
target_compile_definitions(lslobj
	PRIVATE LIBLSL_EXPORTS $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
	INTERFACE LSLNOAUTOLINK # don't use #pragma(lib) in CMake builds
)

# shared library
if (CMAKE_GENERATOR MATCHES "Xcode")
    # Xcode is rather annoying in that it doesn't build SHARED libs unless they have source files.
    # So we need to build from source, and then add all the other settings missing due to lack of lslobj
    add_library(lsl SHARED ${lslobj_sources})
    target_link_libraries(lsl PRIVATE lslboost)
else()
    add_library(lsl SHARED)
    target_link_libraries(lsl
		PUBLIC lslobj
		PRIVATE lslboost)
endif()
target_include_directories(lsl
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
		$<INSTALL_INTERFACE:include>
)
target_compile_definitions(lsl
	PRIVATE LIBLSL_EXPORTS $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
	INTERFACE LSLNOAUTOLINK # don't use #pragma(lib) in CMake builds
)

set_target_properties(lsl PROPERTIES
	VERSION ${liblsl_VERSION_MAJOR}.${liblsl_VERSION_MINOR}.${liblsl_VERSION_PATCH}
)

set(LSL_EXPORT_TARGETS lsl lslobj lslboost lslver)
if(LSL_BUILD_STATIC)
	add_library(lsl-static STATIC)
	target_link_libraries(lsl-static PUBLIC lslobj PRIVATE lslboost)
	# for LSL_CPP_API export header
	target_compile_definitions(lsl-static INTERFACE LIBLSL_STATIC)
	add_executable(lslver-static testing/lslver.c)
	target_link_libraries(lslver-static PRIVATE lsl-static)
	list(APPEND LSL_EXPORT_TARGETS lsl-static)
endif()

if(NOT LSL_NO_FANCY_LIBNAME)
	set(CMAKE_DEBUG_POSTFIX "-debug")
	math(EXPR lslplatform "8 * ${CMAKE_SIZEOF_VOID_P}")
	set_target_properties(lsl PROPERTIES
		PREFIX ""
		OUTPUT_NAME "liblsl${lslplatform}"
		)
endif()

if(LSL_UNIXFOLDERS)
	include(GNUInstallDirs)
else()
	set(CMAKE_INSTALL_BINDIR LSL)
	set(CMAKE_INSTALL_LIBDIR LSL)
	set(CMAKE_INSTALL_INCLUDEDIR LSL/include)
	set(CMAKE_INSTALL_DATAROOTDIR LSL/share)
endif()

add_executable(lslver testing/lslver.c)
target_link_libraries(lslver PRIVATE lsl)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/LSLConfigVersion.cmake"
	VERSION "${liblsl_VERSION_MAJOR}.${liblsl_VERSION_MINOR}.${liblsl_VERSION_PATCH}"
	COMPATIBILITY AnyNewerVersion
)

install(TARGETS ${LSL_EXPORT_TARGETS}
	EXPORT LSLTargets
	COMPONENT liblsl
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

export(EXPORT LSLTargets
	FILE "${CMAKE_CURRENT_BINARY_DIR}/LSLTargets.cmake"
	NAMESPACE LSL::
)

install(EXPORT LSLTargets
	FILE LSLTargets.cmake
	COMPONENT liblsl
	NAMESPACE "LSL::"
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/LSL
)
configure_file(cmake/LSLConfig.cmake "${CMAKE_CURRENT_BINARY_DIR}/LSLConfig.cmake" COPYONLY)
configure_file(cmake/LSLCMake.cmake "${CMAKE_CURRENT_BINARY_DIR}/LSLCMake.cmake" COPYONLY)


# install headers
install(DIRECTORY include/
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	COMPONENT liblsl
)

install(FILES
	cmake/LSLCMake.cmake
	${CMAKE_CURRENT_BINARY_DIR}/LSLConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/LSLConfigVersion.cmake
	COMPONENT liblsl
	DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/LSL
)

include(cmake/LSLCMake.cmake)

if(LSL_UNITTESTS)
	add_subdirectory(testing)
endif()

if(LSL_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

LSLGenerateCPackConfig()
