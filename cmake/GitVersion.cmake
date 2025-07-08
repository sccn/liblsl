# Try to find out which revision is currently checked out
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

# Generate version information string
#  that can be retrieved with the exported lsl_library_info() function
set(LSL_VERSION_INFO "git:${lslgitrevision}/branch:${lslgitbranch}/build:${CMAKE_BUILD_TYPE}/compiler:${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}")
