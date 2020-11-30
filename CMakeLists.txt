cmake_minimum_required (VERSION 3.12)
project (liblsl
	VERSION 1.14.0
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

option(LSL_DEBUGLOG "Enable (lots of) additional debug messages" OFF)
option(LSL_UNIXFOLDERS "Use the unix folder layout for install targets" On)
option(LSL_FORCE_FANCY_LIBNAME "Add library name decorations (32/64/-debug)" OFF)
option(LSL_BUILD_EXAMPLES "Build example programs in examples/" OFF)
option(LSL_BUILD_STATIC "Also build static LSL library." OFF)
option(LSL_LEGACY_CPP_ABI "Build legacy C++ ABI into lsl-static" OFF)
option(LSL_UNITTESTS "Build LSL library unit tests" OFF)
option(LSL_BUNDLED_PUGIXML "Use the bundled pugixml by default" ON)

set(LSL_WINVER "0x0601" CACHE STRING
	"Windows version (_WIN32_WINNT) to target (defaults to 0x0601 for Windows 7)")

# Add an object library so all files are only compiled once
add_library(lslobj OBJECT
	src/api_config.cpp
	src/api_config.h
	src/api_types.hpp
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
	src/inireader.h
	src/inireader.cpp
	src/inlet_connection.cpp
	src/inlet_connection.h
	src/lsl_resolver_c.cpp
	src/lsl_inlet_c.cpp
	src/lsl_outlet_c.cpp
	src/lsl_streaminfo_c.cpp
	src/lsl_xml_element_c.cpp
	src/netinterfaces.h
	src/netinterfaces.cpp
	src/portable_archive/portable_archive_exception.hpp
	src/portable_archive/portable_archive_includes.hpp
	src/portable_archive/portable_iarchive.hpp
	src/portable_archive/portable_oarchive.hpp
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
	thirdparty/loguru/loguru.cpp
	$<$<BOOL:${LSL_LEGACY_CPP_ABI}>:src/legacy/legacy_abi.cpp src/legacy/legacy_abi.h>
	# headers
	include/lsl_c.h
	include/lsl_cpp.h
	include/lsl/common.h
	include/lsl/inlet.h
	include/lsl/outlet.h
	include/lsl/resolver.h
	include/lsl/streaminfo.h
	include/lsl/types.h
	include/lsl/xml.h
)

if(LSL_BUNDLED_PUGIXML)
	message(STATUS "Using bundled pugixml")
	target_sources(lslobj PRIVATE thirdparty/pugixml/pugixml.cpp)
	target_include_directories(lslobj
		PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/pugixml>)
else()
	message(STATUS "Using system pugixml")
	find_package(pugixml REQUIRED)
	target_link_libraries(lslobj PRIVATE pugixml)
endif()

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
set(LSL_VERSION_INFO "git:${lslgitrevision}/branch:${lslgitbranch}/build:${CMAKE_BUILD_TYPE}/compiler:${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}")
set_source_files_properties("thirdparty/loguru/loguru.cpp"
	PROPERTIES COMPILE_DEFINITIONS LOGURU_STACKTRACES=$<BOOL:${LSL_DEBUGLOG}>)

## create the lslboost target
add_library(lslboost OBJECT
	lslboost/asio_objects.cpp
	lslboost/serialization_objects.cpp
)

if(UNIX)
	include(CheckSymbolExists)
	# look for clock_gettime, if not we need to link against librt
	check_symbol_exists(clock_gettime time.h HAS_GETTIME)
	if(NOT HAS_GETTIME)
		set(NEEDS_LIBRT ON CACHE INTERNAL "Link to librt")
	endif()
endif()

find_package(Threads REQUIRED)
target_link_libraries(lslboost
	PUBLIC
		Threads::Threads
	PRIVATE
		$<$<PLATFORM_ID:Windows>:bcrypt>
		$<$<PLATFORM_ID:Windows>:mswsock>
		$<$<PLATFORM_ID:Windows>:ws2_32>
	)
target_compile_features(lslboost PUBLIC cxx_std_11 cxx_lambda_init_captures)

target_compile_definitions(lslboost
	PUBLIC
		BOOST_ALL_NO_LIB
		BOOST_ASIO_STANDALONE
		BOOST_ASIO_SEPARATE_COMPILATION
		BOOST_THREAD_DONT_PROVIDE_INTERRUPTIONS
		$<$<PLATFORM_ID:Windows>:_WIN32_WINNT=${LSL_WINVER}>
	PRIVATE
		$<$<PLATFORM_ID:Windows>:BOOST_THREAD_BUILD_DLL>
)
target_include_directories(lslboost PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lslboost>)


target_link_libraries(lslobj
	PRIVATE
		lslboost
		$<$<PLATFORM_ID:Windows>:iphlpapi>
		$<$<PLATFORM_ID:Windows>:winmm>
	PUBLIC
		$<$<AND:$<BOOL:${LSL_DEBUGLOG}>,$<PLATFORM_ID:Linux>>:dl>
		$<$<BOOL:${NEEDS_LIBRT}>:rt>
	)
target_include_directories(lslobj
	PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/thirdparty/loguru>
)
target_compile_definitions(lslobj
	PRIVATE LIBLSL_EXPORTS $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
		LOGURU_DEBUG_LOGGING=$<BOOL:${LSL_DEBUGLOG}>
	INTERFACE LSLNOAUTOLINK # don't use #pragma(lib) in CMake builds
)

# shared library
add_library(lsl SHARED src/buildinfo.cpp)
target_compile_definitions(lsl PRIVATE
	LSL_LIBRARY_INFO_STR="${LSL_VERSION_INFO}/link:shared"
	LIBLSL_EXPORTS)
target_link_libraries(lsl
	PUBLIC lslobj
	PRIVATE lslboost)
set_target_properties(lsl PROPERTIES
	VERSION ${liblsl_VERSION_MAJOR}.${liblsl_VERSION_MINOR}.${liblsl_VERSION_PATCH}
)

set(LSL_EXPORT_TARGETS lsl lslobj lslboost)
if(LSL_BUILD_STATIC)
	add_library(lsl-static STATIC src/buildinfo.cpp)
	target_compile_definitions(lsl-static PRIVATE LSL_LIBRARY_INFO_STR="${LSL_VERSION_INFO}/link:static")
	target_link_libraries(lsl-static PUBLIC lslobj PRIVATE lslboost)
	list(APPEND LSL_EXPORT_TARGETS lsl-static)
	# for LSL_CPP_API export header
	target_compile_definitions(lsl-static PUBLIC LIBLSL_STATIC)
endif()

if(LSL_FORCE_FANCY_LIBNAME)
	math(EXPR lslplatform "8 * ${CMAKE_SIZEOF_VOID_P}")
	set_target_properties(lsl PROPERTIES
		PREFIX ""
		OUTPUT_NAME "liblsl${lslplatform}"
		DEBUG_POSTFIX "-debug"
	)
	if(LSL_BUILD_STATIC)
		set_target_properties(lsl-static PROPERTIES
			PREFIX ""
			OUTPUT_NAME "liblsl${lslplatform}-static"
			DEBUG_POSTFIX "-debug"
		)
	endif()
endif()

if(LSL_UNIXFOLDERS)
	include(GNUInstallDirs)
else()
	set(CMAKE_INSTALL_BINDIR LSL)
	set(CMAKE_INSTALL_LIBDIR LSL)
	set(CMAKE_INSTALL_INCLUDEDIR LSL/include)
	set(CMAKE_INSTALL_DATAROOTDIR LSL/share)
endif()

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

add_executable(lslver testing/lslver.c)
target_link_libraries(lslver PRIVATE lsl)
installLSLApp(lslver)
if(LSL_BUILD_STATIC)
	add_executable(lslver-static testing/lslver.c)
	target_link_libraries(lslver-static PRIVATE lsl-static)
	installLSLApp(lslver-static)
endif()

set(LSL_INSTALL_ROOT ${CMAKE_CURRENT_BINARY_DIR})
if(LSL_UNITTESTS)
	add_subdirectory(testing)
endif()

if(LSL_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

LSLGenerateCPackConfig()
