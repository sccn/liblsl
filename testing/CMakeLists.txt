project(lsltests
	VERSION 1.14.0
	LANGUAGES CXX
	DESCRIPTION "Unit tests for liblsl"
	)
cmake_minimum_required (VERSION 3.12)
enable_testing()

option(LSL_BENCHMARKS "Enable benchmarks in unit tests" OFF)

add_library(catch_main OBJECT catch_main.cpp)
target_compile_features(catch_main PUBLIC cxx_std_11)

target_compile_definitions(catch_main PRIVATE LSL_VERSION_INFO="${LSL_VERSION_INFO}")
if(LSL_BENCHMARKS)
	target_compile_definitions(catch_main PUBLIC CATCH_CONFIG_ENABLE_BENCHMARKING)
endif()
target_include_directories(catch_main PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/Catch2>)

add_executable(lsl_test_exported
	test_ext_DataType.cpp
	test_ext_discovery.cpp
	test_ext_move.cpp
	test_ext_streaminfo.cpp
	test_ext_time.cpp
)
target_link_libraries(lsl_test_exported PRIVATE lsl catch_main)

find_package(Threads REQUIRED)

add_executable(lsl_test_internal
	test_int_inireader.cpp
	test_int_network.cpp
	test_int_stringfuncs.cpp
	test_int_streaminfo.cpp
	test_int_samples.cpp
	internal/serialization_v100.cpp
)
target_link_libraries(lsl_test_internal PRIVATE lslobj lslboost catch_main)
target_include_directories(lsl_test_internal PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../src/)

if(LSL_BENCHMARKS)
	# to get somewhat reproducible performance numbers:
	# /usr/bin/time -v testing/lsl_bench_exported --benchmark-samples 100 bounce
	# [unix only]     |   binary                  |    nr. of samples   | test name
	add_executable(lsl_bench_exported
		bench_ext_bounce.cpp
		bench_ext_common.cpp
		bench_ext_pushpull.cpp
	)
	target_link_libraries(lsl_bench_exported PRIVATE lsl catch_main Threads::Threads)
	installLSLApp(lsl_bench_exported)

	add_executable(lsl_bench_internal
		bench_int_sleep.cpp
	)
	target_link_libraries(lsl_bench_internal PRIVATE lslobj lslboost catch_main)
	installLSLApp(lsl_bench_internal)
endif()

set(LSL_TESTS lsl_test_exported lsl_test_internal)
foreach(lsltest ${LSL_TESTS})
	add_test(NAME ${lsltest} COMMAND ${lsltest} --wait-for-keypress never)
	installLSLApp(${lsltest})
endforeach()

installLSLAuxFiles(lsl_test_exported directory lslcfgs)
