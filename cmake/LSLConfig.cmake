if(TARGET lsl AND NOT TARGET LSL::lsl)
	add_library(LSL::lsl ALIAS lsl)
endif()

if(NOT TARGET LSL::lsl)
	include("${CMAKE_CURRENT_LIST_DIR}/LSLTargets.cmake")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/LSLCMake.cmake")
