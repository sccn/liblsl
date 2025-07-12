include(cmake/LSLCMake.cmake)  # Needed for `installLSLApp`

# Build utilities
add_executable(lslver testing/lslver.c)
target_link_libraries(lslver PRIVATE lsl)
installLSLApp(lslver)

if(NOT WIN32 AND LSL_TOOLS)
    add_executable(blackhole testing/blackhole.cpp)
    target_link_libraries(blackhole PRIVATE Threads::Threads)
    target_include_directories(blackhole PRIVATE "thirdparty/asio/")
    installLSLApp(blackhole)
endif()
