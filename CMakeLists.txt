cmake_minimum_required (VERSION 3.12)
project (LSL
	VERSION 1.13.0
	LANGUAGES C CXX)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE On)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_DEBUG_POSTFIX)
	set(CMAKE_DEBUG_POSTFIX -debug)
endif()
option(LSL_UNIXFOLDERS "Use the unix folder layout for install targets" On)
option(LSL_NO_FANCY_LIBNAME "Skip library name decorations (32/64/-debug)")
option(LSL_BUILD_EXAMPLES "Build example programs in examples/" OFF)
option(LSL_BUILD_STATIC "Also build static LSL library." OFF)
option(LSL_LEGACY_CPP_ABI "Build legacy C++ ABI into lsl-static" OFF)
option(LSL_UNITTESTS "Build LSL library unit tests" OFF)

set(LSL_WINVER "0x0601" CACHE STRING
	"Windows version (_WIN32_WINNT) to target (defaults to 0x0601 for Windows 7)")

# Add an object library so all files are only compiled once
add_library(lslobj OBJECT
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
)
if(LSL_LEGACY_CPP_ABI)
	list(APPEND sources src/legacy/legacy_abi.cpp src/legacy/legacy_abi.h)
endif()

set(headers
	${CMAKE_CURRENT_LIST_DIR}/include/lsl_c.h
	${CMAKE_CURRENT_LIST_DIR}/include/lsl_constants.h
	${CMAKE_CURRENT_LIST_DIR}/include/lsl_cpp.h
)
target_sources(lslobj PRIVATE ${headers})

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(lslplatform 64)
else()
	set(lslplatform 32)
endif()

if(LSL_NO_FANCY_LIBNAME)
	set(lslplatform "")
	set(CMAKE_DEBUG_POSTFIX "")
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
else ()  # WIN32
	target_sources(lslboost PRIVATE
		lslboost/libs/serialization/src/codecvt_null.cpp
		lslboost/libs/thread/src/win32/thread.cpp
		lslboost/libs/thread/src/win32/tss_dll.cpp
		lslboost/libs/thread/src/win32/tss_pe.cpp
	)
        target_link_libraries(lslboost PRIVATE bcrypt)
        target_compile_definitions(lslboost
        PUBLIC
                _WIN32_WINNT=${LSL_WINVER}
        PRIVATE
                BOOST_THREAD_BUILD_DLL
        )
	if(MINGW)
		target_link_libraries(lslboost PUBLIC wsock32 ws2_32 winmm)
	endif()
endif ()

target_compile_definitions(lslboost PUBLIC
	BOOST_ALL_NO_LIB
	BOOST_ASIO_SEPARATE_COMPILATION
)
target_include_directories(lslboost PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lslboost>)



target_link_libraries(lslobj PRIVATE lslboost)
target_include_directories(lslobj
	PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
)
target_compile_definitions(lslobj
	PRIVATE LIBLSL_EXPORTS _SCL_SECURE_NO_WARNINGS _CRT_SECURE_NO_WARNINGS
	PRIVATE $<TARGET_PROPERTY:lslboost,INTERFACE_COMPILE_DEFINITIONS>
	INTERFACE LSLNOAUTOLINK # don't use #pragma(lib) in CMake builds
)
if(NOT MSVC_VERSION VERSION_LESS 1700)
	target_compile_features(lslobj PUBLIC cxx_auto_type)
endif()

# shared library
add_library(lsl SHARED)
target_link_libraries(lsl PUBLIC lslobj PRIVATE lslboost)
set_target_properties(lsl PROPERTIES
	VERSION ${LSL_VERSION_MAJOR}.${LSL_VERSION_MINOR}.${LSL_VERSION_PATCH}
	PREFIX ""
	OUTPUT_NAME "liblsl${lslplatform}"
)

if(LSL_UNITTESTS OR LSL_BUILD_STATIC)
	add_library(lsl-static STATIC)
	target_link_libraries(lsl-static PUBLIC lslobj PRIVATE lslboost)
	# for LSL_CPP_API export header
	target_compile_definitions(lsl-static PUBLIC LIBLSL_STATIC)
endif()

if(LSL_UNIXFOLDERS)
	set(LSLPREFIX "")
else()
	set(LSLPREFIX "LSL/")
endif()

add_executable(lslver testing/lslver.c)
target_link_libraries(lslver PRIVATE lsl)

set(LSL_EXPORT_TARGETS "lsl;lslobj;lslboost;lslver")
if(LSL_BUILD_STATIC)
	list(APPEND LSL_EXPORT_TARGETS "lsl-static")
endif()

install(TARGETS ${LSL_EXPORT_TARGETS}
	COMPONENT liblsl
	EXPORT "${PROJECT_NAME}Config"
	RUNTIME DESTINATION ${LSLPREFIX}bin
	LIBRARY DESTINATION ${LSLPREFIX}lib
	INCLUDES DESTINATION ${LSLPREFIX}include
	ARCHIVE DESTINATION ${LSLPREFIX}lib
)

install(EXPORT "${PROJECT_NAME}Config"
	COMPONENT liblsl
	NAMESPACE "LSL::"
	DESTINATION "${LSLPREFIX}share/LSL")

# install headers
install(FILES ${headers}
	COMPONENT liblsl
	DESTINATION "${LSLPREFIX}include"
)

install(FILES LSLCMake.cmake
	COMPONENT liblsl
	DESTINATION "${LSLPREFIX}share/LSL")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
include(LSLCMake)

if(LSL_UNITTESTS)
	add_subdirectory(testing)
endif()

if(LSL_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

LSLGenerateCPackConfig()
