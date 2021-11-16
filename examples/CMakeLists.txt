cmake_minimum_required(VERSION 3.12)
project(lslexamples
	LANGUAGES C CXX
	VERSION 0.2.0)
find_package(LSL REQUIRED
	HINTS ${LSL_INSTALL_ROOT}
	"${CMAKE_CURRENT_LIST_DIR}/../build"
	"${CMAKE_CURRENT_LIST_DIR}/../out/x64-Release"
	PATH_SUFFIXES share/LSL)
get_filename_component(LSL_PATH ${LSL_CONFIG} DIRECTORY)
message(STATUS "Found LSL lib in ${LSL_PATH}")
message(STATUS "LSL BIN DIR: ${liblsl_BINARY_DIR} & ${LIBLSL_BINARY_DIR}")

# convenience function to add an example file
# this creates a target, links the necessary libraries and
# creates an install target
function(addlslexample name extension)
	add_executable(${name}
		${name}.${extension}
	)
	target_link_libraries(${name} PRIVATE LSL::lsl)
	target_compile_features(${name} PRIVATE cxx_constexpr)
	installLSLApp(${name})
endfunction()

find_package(Threads)

addlslexample(GetAllStreams cpp)
addlslexample(GetFullinfo cpp)
addlslexample(GetTimeCorrection cpp)
addlslexample(HandleMetaData cpp)
addlslexample(HandleMetaDataC c)
addlslexample(ReceiveData cpp)
addlslexample(ReceiveDataC c)
addlslexample(ReceiveDataInChunks cpp)
addlslexample(ReceiveDataSimple cpp)
addlslexample(ReceiveStringMarkers cpp)
addlslexample(ReceiveStringMarkersC c)
addlslexample(SendData cpp)
addlslexample(SendDataC c)
addlslexample(SendDataInChunks cpp)
addlslexample(SendDataSimple cpp)
addlslexample(SendMultipleStreams cpp)
addlslexample(SendStringMarkers cpp)
addlslexample(SendStringMarkersC c)
addlslexample(TestSyncWithoutData cpp)

target_link_libraries(TestSyncWithoutData PRIVATE Threads::Threads)

