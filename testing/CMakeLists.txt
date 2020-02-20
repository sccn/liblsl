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

target_compile_definitions(catch_main PRIVATE LSL_VERSION_INFO=${LSL_VERSION_INFO})
if(LSL_BENCHMARKS)
	target_compile_definitions(catch_main PUBLIC CATCH_CONFIG_ENABLE_BENCHMARKING)
endif()

add_executable(lsl_test_exported
	DataType.cpp
	discovery.cpp
	move.cpp
	timesync.cpp
)
target_link_libraries(lsl_test_exported PRIVATE lsl catch_main)

add_executable(lsl_test_internal
	asiocancel.cpp
	inireader.cpp
	stringfuncs.cpp
	streaminfo.cpp
)
target_link_libraries(lsl_test_internal PRIVATE lslobj lslboost catch_main)

set(LSL_TESTS lsl_test_exported lsl_test_internal)
foreach(lsltest ${LSL_TESTS})
	add_test(NAME ${lsltest} COMMAND ${lsltest})
	installLSLApp(${lsltest})
endforeach()
