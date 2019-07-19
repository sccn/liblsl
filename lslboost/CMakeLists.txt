# Platform-independent sources
add_library (lslboost STATIC
	asio_objects.cpp
	libs/atomic/src/lockpool.cpp
	libs/chrono/src/chrono.cpp
	libs/serialization/src/archive_exception.cpp
	libs/serialization/src/basic_archive.cpp
	libs/serialization/src/basic_iarchive.cpp
	libs/serialization/src/basic_iserializer.cpp
	libs/serialization/src/basic_oarchive.cpp
	libs/serialization/src/basic_oserializer.cpp
	libs/serialization/src/basic_serializer_map.cpp
	libs/serialization/src/extended_type_info.cpp
	libs/serialization/src/extended_type_info_typeid.cpp
	libs/serialization/src/void_cast.cpp
)
if (UNIX)
	target_sources(lslboost PRIVATE
		libs/thread/src/pthread/once.cpp
		libs/thread/src/pthread/thread.cpp
	)
	find_package(Threads REQUIRED)
	target_link_libraries(lslboost PRIVATE Threads::Threads)
	if(NOT APPLE AND NOT CMAKE_ANDROID_ARCH_ABI)
		target_link_libraries(lslboost PRIVATE rt)
	endif()
else ()  # WIN32
	target_sources(lslboost PRIVATE
		libs/serialization/src/codecvt_null.cpp
		libs/thread/src/win32/thread.cpp
		libs/thread/src/win32/tss_dll.cpp
		libs/thread/src/win32/tss_pe.cpp
	)
	if(MINGW)
		target_link_libraries(lslboost PRIVATE wsock32 ws2_32 winmm)
	endif()
endif ()

target_compile_definitions(lslboost PUBLIC
	BOOST_ALL_NO_LIB
	BOOST_ASIO_SEPARATE_COMPILATION
	_WIN32_WINNT=${LSL_WINVER}
	BOOST_UUID_RANDOM_PROVIDER_FORCE_WINCRYPT
	PRIVATE
	BOOST_THREAD_BUILD_DLL
)

target_include_directories(lslboost SYSTEM PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
set_target_properties(lslboost PROPERTIES
	CXX_VISIBILITY_PRESET hidden
	POSITION_INDEPENDENT_CODE On
)

# gcc 5 needs -std=c++0x to compile lslboost
if(NOT MSVC)
  target_compile_features(lslboost PRIVATE cxx_auto_type)
endif ()

export(TARGETS lslboost
	FILE lslboostConfig.cmake)
