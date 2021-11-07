project(lsltests
	VERSION 1.14.1
	LANGUAGES CXX
	DESCRIPTION "Unit tests for liblsl"
	)
cmake_minimum_required (VERSION 3.12)
enable_testing()

option(LSL_BENCHMARKS "Enable benchmarks in unit tests" OFF)

add_library(catch_main OBJECT catch_main.cpp)
target_compile_features(catch_main PUBLIC cxx_std_14)
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
	target_link_libraries(catch_main PUBLIC log)
endif()
find_package(Threads REQUIRED)
target_link_libraries(catch_main PUBLIC Threads::Threads)

target_compile_definitions(catch_main PRIVATE LSL_VERSION_INFO="${LSL_VERSION_INFO}")
target_include_directories(catch_main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty)
if(LSL_BENCHMARKS)
	target_compile_definitions(catch_main PUBLIC CATCH_CONFIG_ENABLE_BENCHMARKING)
endif()

add_library(common OBJECT
	common/bytecmp.cpp
	common/bytecmp.hpp
	common/create_streampair.hpp
	common/lsltypes.hpp
)
target_compile_features(common PUBLIC cxx_std_14)
target_include_directories(common PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../src>
)


add_executable(lsl_test_exported
	ext/DataType.cpp
	ext/discovery.cpp
	ext/move.cpp
	ext/streaminfo.cpp
	ext/time.cpp
)
target_link_libraries(lsl_test_exported PRIVATE lsl common catch_main)


add_executable(lsl_test_internal
	int/inireader.cpp
	int/loguruthreadnames.cpp
	int/network.cpp
	int/stringfuncs.cpp
	int/streaminfo.cpp
	int/samples.cpp
	int/postproc.cpp
	int/serialization_v100.cpp
	int/tcpserver.cpp
)
target_link_libraries(lsl_test_internal PRIVATE lslobj lslboost common catch_main)

if(LSL_BENCHMARKS)
	# to get somewhat reproducible performance numbers:
	# /usr/bin/time -v testing/lsl_test_exported --benchmark-samples 100  bounce
	# [unix only]     |   binary                |    nr. of samples      | test name
	target_sources(lsl_test_exported PRIVATE
		ext/bench_bounce.cpp
		ext/bench_common.cpp
		ext/bench_pushpull.cpp
	)
	target_sources(lsl_test_internal PRIVATE
		int/bench_sleep.cpp
		int/bench_timesync.cpp
	)
endif()

set(LSL_TESTS lsl_test_exported lsl_test_internal)
foreach(lsltest ${LSL_TESTS})
	add_test(NAME ${lsltest} COMMAND ${lsltest} --wait-for-keypress never)
	installLSLApp(${lsltest})
endforeach()

installLSLAuxFiles(lsl_test_exported directory lslcfgs)
