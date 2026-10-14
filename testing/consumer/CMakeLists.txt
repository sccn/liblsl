# A minimal application that uses liblsl the way downstream projects do.
# Used by .github/workflows/install-and-link.yml (see testing/consumer/test_consumers.sh).
#   -DLSL_SOURCE_DIR=<liblsl checkout>  : get liblsl with FetchContent and build it in-tree
#   otherwise                           : find an installed liblsl with find_package(LSL)
cmake_minimum_required(VERSION 3.23)
project(lsl_consumer LANGUAGES C CXX)

# Put executables and (in-tree) DLLs side by side so Windows finds lsl.dll.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(LSL_SOURCE_DIR)
    include(FetchContent)
    FetchContent_Declare(liblsl SOURCE_DIR "${LSL_SOURCE_DIR}")
    FetchContent_MakeAvailable(liblsl)
else()
    find_package(LSL REQUIRED)
    message(STATUS "Found LSL ${LSL_VERSION} in ${LSL_DIR}")
endif()

add_executable(consumer_cpp main.cpp)
target_link_libraries(consumer_cpp PRIVATE LSL::lsl)
add_executable(consumer_c main.c)
target_link_libraries(consumer_c PRIVATE LSL::lsl)
