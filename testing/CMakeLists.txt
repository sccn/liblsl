project(lsltests
	VERSION 1.13.1
	LANGUAGES CXX
	DESCRIPTION "Unit tests for liblsl"
	)
cmake_minimum_required (VERSION 3.12)

enable_testing()
add_library(catch_main OBJECT catch_main.cpp)
target_compile_features(catch_main PUBLIC cxx_std_11)
add_executable(lsl_test_exported
	DataType.cpp
	discovery.cpp
	timesync.cpp
)
target_link_libraries(lsl_test_exported PRIVATE lsl catch_main)

add_executable(lsl_test_internal
	asiocancel.cpp
)
target_link_libraries(lsl_test_internal PRIVATE lslobj lslboost catch_main)

set(LSL_TESTS lsl_test_exported lsl_test_internal)
foreach(lsltest ${LSL_TESTS})
	add_test(NAME ${lsltest} COMMAND ${lsltest})
	installLSLApp(${lsltest})
endforeach()
