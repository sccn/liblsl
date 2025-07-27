include(cmake/LSLCMake.cmake)  # Needed for `installLSLApp` and `LSLGenerateCPackConfig`

    # Build utilities
add_executable(lslver testing/lslver.c)
target_link_libraries(lslver PRIVATE lsl)
if(NOT IOS)
installLSLApp(lslver)
endif(NOT IOS)
if(NOT WIN32 AND LSL_TOOLS)
    add_executable(blackhole testing/blackhole.cpp)
    target_link_libraries(blackhole PRIVATE Threads::Threads)
    target_include_directories(blackhole PRIVATE "thirdparty/asio/")
    if(NOT IOS)
        installLSLApp(blackhole)
    endif(NOT IOS)
endif()
