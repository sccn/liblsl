project(lsltests
	VERSION 1.14.1
	LANGUAGES CXX
	DESCRIPTION "Unit tests for liblsl"
	)
cmake_minimum_required (VERSION 3.12)
enable_testing()

option(LSL_BENCHMARKS "Enable benchmarks in unit tests" OFF)


Include(FetchContent)
FetchContent_Declare(
		Catch2
		GIT_REPOSITORY https://github.com/catchorg/Catch2.git
		GIT_TAG        v3.4.0 # or a later release
)
FetchContent_MakeAvailable(Catch2)


add_library(catch_main OBJECT catch_main.cpp)
target_compile_features(catch_main PUBLIC cxx_std_14)
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
	target_link_libraries(catch_main PUBLIC log)
endif()
find_package(Threads REQUIRED)
target_link_libraries(catch_main
	PUBLIC
		Threads::Threads
		Catch2::Catch2
)

target_compile_definitions(catch_main PRIVATE LSL_VERSION_INFO="${LSL_VERSION_INFO}")
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
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../src>
)
target_link_libraries(common PUBLIC Catch2::Catch2)


add_executable(lsl_test_exported
	ext/DataType.cpp
	ext/discovery.cpp
	ext/move.cpp
	ext/streaminfo.cpp
	ext/time.cpp
)
target_link_libraries(lsl_test_exported PRIVATE lsl common catch_main)


set(LSL_TEST_INTERNAL_SRCS
		int/inireader.cpp
		int/network.cpp
		int/stringfuncs.cpp
		int/streaminfo.cpp
		int/samples.cpp
		int/postproc.cpp
		int/serialization_v100.cpp
		int/tcpserver.cpp
)
if(NOT MINGW)
	LIST(APPEND LSL_INTERNAL_SRCS int/loguruthreadnames.cpp)
endif()
message(STATUS ${LSL_TEST_INTERNAL_SRCS})
add_executable(lsl_test_internal ${LSL_TEST_INTERNAL_SRCS})
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

#	# Uncomment me if debugging tests on Windows. Commented because it does not work well on CI runners.
#	if(WIN32)
#		# Copy dependencies into build directory to enable debugging builds in Windows.
#		add_custom_command(TARGET ${lsltest}
#			POST_BUILD
#			COMMAND
#				${CMAKE_COMMAND} -E copy_if_different
#					$<TARGET_RUNTIME_DLLS:${lsltest}>
#					$<TARGET_FILE_DIR:${lsltest}>
#			COMMAND_EXPAND_LISTS
#		)
#	endif(WIN32)
endforeach()

installLSLAuxFiles(lsl_test_exported directory lslcfgs)
