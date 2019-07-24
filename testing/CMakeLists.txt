cmake_minimum_required (VERSION 3.12)

configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
	RESULT_VARIABLE result
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
	message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
	RESULT_VARIABLE result
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
	message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
	${CMAKE_CURRENT_BINARY_DIR}/googletest-build
	EXCLUDE_FROM_ALL)

set(LSL_TESTS lsl_test_exported)

enable_testing()
add_executable(lsl_test_exported
	DataType.cpp
	discovery.cpp
	timesync.cpp
)
target_link_libraries(lsl_test_exported PRIVATE lsl gtest_main)

if(TARGET lsl-static)
	add_executable(lsl_test_internal
			asiocancel.cpp
	)
	target_link_libraries(lsl_test_internal PRIVATE lsl-static lslboost gtest_main)
	list(APPEND LSL_TESTS lsl_test_internal)
else()
	message(WARNING "Skipping internal tests because the static library isn't build")
endif()

foreach(lsltest ${LSL_TESTS})
	add_test(NAME ${lsltest} COMMAND ${lsltest})
endforeach()

install(TARGETS ${LSL_TESTS}
	COMPONENT liblsl-tests
	RUNTIME DESTINATION ${LSLPREFIX}bin)
