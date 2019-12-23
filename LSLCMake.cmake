# Common functions and settings for LSL

message(STATUS "Included LSL CMake helpers, rev. 11")

# set build type and default install dir if not done already
if(NOT CMAKE_BUILD_TYPE)
	message(STATUS "CMAKE_BUILD_TYPE was default initialized to Release")
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
# OR ((${MSVC_VERSION} GREATER_EQUAL 1910) AND ("${CMAKE_GENERATOR}" STREQUAL "Ninja"))
	set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH
		"Where to put redistributable binaries" FORCE)
	message(WARNING "CMAKE_INSTALL_PREFIX default initialized to ${CMAKE_INSTALL_PREFIX}")
endif()

# Try to find the labstreaminglayer library and enable
# the imported target LSL::lsl
#
# Use it with
# target_link_libraries(your_target_app PRIVATE LSL::lsl)

if(TARGET lsl)
	add_library(LSL::lsl ALIAS lsl)
	message(STATUS "Found target lsl in current build tree")
else()
	message(STATUS "Trying to find package LSL")
	list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR})
	find_package(LSL REQUIRED)
endif()

# Generate folders for IDE targets (e.g., VisualStudio solutions)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set runtime path, i.e. where shared libs are searched relative to the exe
if(APPLE)
	list(APPEND CMAKE_INSTALL_RPATH "@executable_path/../LSL/lib")
	list(APPEND CMAKE_INSTALL_RPATH "@executable_path/../lib")
	list(APPEND CMAKE_INSTALL_RPATH "@executable_path/")
elseif(UNIX)
	list(APPEND CMAKE_INSTALL_RPATH "\$ORIGIN/../LSL/lib:\$ORIGIN/../lib/:\$ORIGIN")
endif()

set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "limited configs" FORCE)

# Qt5
set(CMAKE_INCLUDE_CURRENT_DIR ON) # Because the ui_mainwindow.h file.

# Boost
#SET(Boost_DEBUG OFF) #Switch this and next to ON for help debugging Boost problems.
#SET(Boost_DETAILED_FAILURE_MSG ON)
if(WIN32)
	set(Boost_USE_STATIC_LIBS ON)
endif()

# LSL functions, mostly for Apps

# installs additional files (configuration etc.)
function(installLSLAuxFiles target)
	get_target_property(is_bundle ${target} MACOSX_BUNDLE)
	set(destdir ${PROJECT_NAME})
	if(LSL_UNIXFOLDERS)
		set(destdir bin/)
	elseif(is_bundle AND APPLE)
		set(destdir ${destdir}/${target}.app/Contents/MacOS)
	endif()
	if("${ARGV1}" STREQUAL "directory")
		install(DIRECTORY ${ARGV2} DESTINATION ${destdir} COMPONENT "${PROJECT_NAME}")
	else()
		install(FILES ${ARGN} DESTINATION ${destdir} COMPONENT "${PROJECT_NAME}")
	endif()
endfunction()

# installLSLApp: adds the specified target to the install list
#
# there are two install types for different use cases
# Windows users mostly want a folder per app that contains
# everything needed to run the app, whereas on Linux,
# OS X (homebrew) and Conda the libraries are installed
# separately to save space, ease upgrading and distribution
function(installLSLApp target)
	get_target_property(TARGET_LIBRARIES ${target} LINK_LIBRARIES)
	string(REGEX MATCH ";Qt5::" qtapp ";${TARGET_LIBRARIES}")
	if(qtapp)
		# Enable automatic compilation of .cpp->.moc, xy.ui->ui_xy.h and resource files
		set_target_properties(${target} PROPERTIES
			AUTOMOC ON
			AUTOUIC ON
			AUTORCC ON
		)
	endif()
	if(LSL_UNIXFOLDERS)
		install(TARGETS ${target}
			COMPONENT "${PROJECT_NAME}"
			RUNTIME DESTINATION bin
			BUNDLE  DESTINATION bin
			LIBRARY DESTINATION lib
		)
	else()
		installLSLAppSingleFolder(${target} "${qtapp}")
	endif()
	set_property(GLOBAL APPEND PROPERTY
		"LSLDEPENDS_${PROJECT_NAME}" liblsl)
endfunction()

function(findQtInstallationTool qtdeploytoolname)
	if(QT_DEPLOYQT_EXECUTABLE)
		return()
	endif()
	get_target_property(QT_QMAKE_EXE Qt5::qmake IMPORTED_LOCATION)
	get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXE}" DIRECTORY)
	find_program (QT_DEPLOYQT_EXECUTABLE ${qtdeploytoolname} HINTS "${QT_BIN_DIR}")
	if (QT_DEPLOYQT_EXECUTABLE)
		message(STATUS "Qt deploy tool found at ${QT_DEPLOYQT_EXECUTABLE}")
	else()
		message(WARNING "Windeployqt wasn't found, installing ${PROJECT_NAME} will fail!")
		return()
	endif()
endfunction()

# installLSLAppSingleFolder: installs the app its folder and copies needed libraries
#
# when calling make install / ninja install the executable is installed to
# CMAKE_INSTALL_PREFIX/PROJECT_NAME/TARGET_NAME
# e.g. C:/LSL/BrainAmpSeries/BrainAmpSeries.exe
function(installLSLAppSingleFolder target deployqt)
	install(TARGETS ${target}
		COMPONENT "${PROJECT_NAME}"
		BUNDLE DESTINATION ${PROJECT_NAME}
		RUNTIME DESTINATION ${PROJECT_NAME}
		LIBRARY DESTINATION ${PROJECT_NAME}/lib
	)
	set(appbin "${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}/${target}${CMAKE_EXECUTABLE_SUFFIX}")

	# Copy lsl library for WIN32 or MacOS.
	# On Mac, dylib is only needed for macdeployqt and for non bundles when not using system liblsl.
	# Copy anyway, and fixup_bundle can deal with the dylib already being present.
	if(WIN32 OR APPLE)
		installLSLAuxFiles(${target} $<TARGET_FILE:LSL::lsl>)
		set_property(INSTALL "${PROJECT_NAME}/$<TARGET_FILE_NAME:${target}>" PROPERTY
			CPACK_START_MENU_SHORTCUTS "${target}")
	endif()

	# do we need to install with Qt5?
	if(deployqt)
		if(WIN32)
			findQtInstallationTool("windeployqt")
			if (QT_DEPLOYQT_EXECUTABLE)
				file (TO_NATIVE_PATH "${QT_BIN_DIR}" QT_BIN_DIR_NATIVE)
				# It's safer to use `\` separators in the Path, but we need to escape them
				string (REPLACE "\\" "\\\\" QT_BIN_DIR_NATIVE "${QT_BIN_DIR_NATIVE}")
				set(QT_DEPLOYQT_FLAGS --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime)
				file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${target}_$<CONFIG>_path"
					CONTENT "$<TARGET_FILE:${target}>"  # Full path to .exe file
				)
				get_filename_component(appdir ${appbin} DIRECTORY CACHE)
				install (CODE "
					file(READ \"${CMAKE_CURRENT_BINARY_DIR}/${target}_${CMAKE_BUILD_TYPE}_path\" _file)
					message (STATUS \"Running Qt Deploy Tool for \${_file}\")
					if (CMAKE_INSTALL_CONFIG_NAME STREQUAL \"Debug\")
						set(QT_DEPLOYQT_FLAGS \"\${QT_DEPLOYQT_FLAGS} --debug\")
					else ()
						set(QT_DEPLOYQT_FLAGS \"\${QT_DEPLOYQT_FLAGS} --release\")
					endif ()
					execute_process(COMMAND
						\"${CMAKE_COMMAND}\" -E env
						\"Path=${QT_BIN_DIR_NATIVE};\$ENV{SystemRoot}\\\\System32;\$ENV{SystemRoot}\"
						\"${QT_DEPLOYQT_EXECUTABLE}\"
						${QT_DEPLOYQT_FLAGS} --dry-run --list mapping
						\"${appbin}\"
						OUTPUT_VARIABLE output
						OUTPUT_STRIP_TRAILING_WHITESPACE
					)
					string(REPLACE \"\\\\\" \"/\" _output \${output})
					separate_arguments(_files WINDOWS_COMMAND \${_output})
					while(_files)
						list(GET _files 0 _src)
						list(GET _files 1 _dest)
						execute_process(
							COMMAND \"${CMAKE_COMMAND}\" -E
								copy_if_different \${_src} \"\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}/$\{_dest}\"
						)
						list(REMOVE_AT _files 0 1)
					endwhile()
					" COMPONENT ${target})
					#add_custom_command(TARGET ${target} POST_BUILD
					#	COMMAND "${CMAKE_COMMAND}" -E env PATH="${QT_BIN_DIR}" "${QT_DEPLOYQT_EXECUTABLE}"
					#	${QT_DEPLOYQT_FLAGS}
					#	\"$<TARGET_FILE:${target}>\")
			endif()
		elseif(APPLE)
			# It should be enough to call fixup_bundle (see below),
			# but this fails to install qt plugins (cocoa).
			# Use macdeployqt instead (but this is bad at grabbing lsl dylib, so we did that above)
			findQtInstallationTool("macdeployqt")
			if(QT_DEPLOYQT_EXECUTABLE)
				set(QT_DEPLOYQT_FLAGS "-verbose=1")  # Adding -libpath=${CMAKE_INSTALL_PREFIX}/LSL/lib seems to do nothing, maybe deprecated
				install(CODE "
					message(STATUS \"Running Qt Deploy Tool...\")
					#list(APPEND QT_DEPLOYQT_FLAGS -dmg)
					if(CMAKE_INSTALL_CONFIG_NAME STREQUAL \"Debug\")
					    list(APPEND QT_DEPLOYQT_FLAGS -use-debug-libs)
					endif()
					execute_process(COMMAND
						\"${QT_DEPLOYQT_EXECUTABLE}\"
						\"${appbin}.app\"
						${QT_DEPLOYQT_FLAGS}
					)
				")
			endif()
		endif()
	elseif(APPLE)
		# fixup_bundle appears to be broken for Qt apps. Use only for non-Qt.
		get_target_property(target_is_bundle ${target} MACOSX_BUNDLE)
		if(target_is_bundle)
			install(CODE "
				include(BundleUtilities)
				set(BU_CHMOD_BUNDLE_ITEMS ON)
				fixup_bundle(
					${appbin}.app
					\"\"
					\"${CMAKE_INSTALL_PREFIX}/LSL/lib\"
				)
				"
				COMPONENT ${PROJECT_NAME}
			)
		endif()
	endif()
endfunction()


# default paths, versions and magic to guess it on windows
# guess default paths for Windows / VC

# Boost autoconfig:
# Original author: Ryan Pavlik <ryan@sensics.com> <ryan.pavlik@gmail.com
# Released with the same license as needed to integrate into CMake.
# Modified by Chadwick Boulay Jan 2018

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(lslplatform 64)
else()
	set(lslplatform 32)
endif()

if(WIN32 AND MSVC)
	# see https://cmake.org/cmake/help/latest/variable/MSVC_VERSION.html
	if(MSVC_VERSION EQUAL 1500)
		set(VCYEAR 2008)
		set(_vs_ver 9.0)
	elseif(MSVC_VERSION EQUAL 1600)
		set(VCYEAR 2010)
		set(_vs_ver 10.0)
	elseif(MSVC_VERSION EQUAL 1700)
		set(VCYEAR 2012)
		set(_vs_ver 11.0)
	elseif(MSVC_VERSION EQUAL 1800)
		set(VCYEAR 2013)
		set(_vs_ver 12.0)
	elseif(MSVC_VERSION EQUAL 1900)
		set(VCYEAR 2015)
		set(_vs_ver 14.0)
	elseif(MSVC_VERSION GREATER 1910 AND MSVC_VERSION LESS 1929)
		set(VCYEAR 2017)
		set(_vs_ver 14.1)
		# Also VS 2019, but it's binary compatible with 2017 so Boost
		# and Qt still use the 2017 binaries
	else()
		message(WARNING "You're using an untested Visual C++ compiler (MSVC_VERSION: ${MSVC_VERSION}).")
	endif()

	if(NOT Qt5_DIR)
		message(STATUS "You didn't specify a Qt5_DIR.")
		file(GLOB_RECURSE Qt5ConfGlobbed
			LIST_DIRECTORIES true
			"C:/Qt/5.1*/msvc${VCYEAR}_${lslplatform}/lib/cmake/Qt5/")
		list(FILTER Qt5ConfGlobbed INCLUDE REGEX "Qt5$")
		if(Qt5ConfGlobbed)
			message(STATUS "Found Qt directories: ${Qt5ConfGlobbed}")
			list(SORT Qt5ConfGlobbed)
			list(GET Qt5ConfGlobbed -1 Qt5_DIR)
			set(Qt5_DIR ${Qt5_DIR} CACHE PATH "Qt5 dir")
		endif()
		message(STATUS "If this is wrong and you are building Apps that require Qt, please add the correct dir:")
		message(STATUS "  -DQt5_DIR=/path/to/Qt5/5.x.y/msvc_${VCYEAR}_${lslplatform}/lib/cmake/Qt5")
	endif()

	if((NOT BOOST_ROOT) AND (NOT Boost_INCLUDE_DIR) AND (NOT Boost_LIBRARY_DIR))
		message(STATUS "Attempting to find Boost, whether or not you need it.")
		set(_libdir "lib${lslplatform}-msvc-${_vs_ver}")
		set(_haslibs)
		if(EXISTS "c:/local")
			file(GLOB _possibilities "c:/local/boost*")
			list(REVERSE _possibilities)
			foreach(DIR ${_possibilities})
				if(EXISTS "${DIR}/${_libdir}")
					list(APPEND _haslibs "${DIR}")
				endif()
			endforeach()
			if(_haslibs)
				list(APPEND CMAKE_PREFIX_PATH ${_haslibs})
				find_package(Boost QUIET)
				if(Boost_FOUND AND NOT Boost_LIBRARY_DIR)
					set(BOOST_ROOT "${Boost_INCLUDE_DIR}" CACHE PATH "")
					set(BOOST_LIBRARYDIR "${Boost_INCLUDE_DIR}/${_libdir}" CACHE PATH "")
				endif()
			endif()
		endif()
	endif()
	if(NOT BOOST_ROOT)
		message(STATUS "Did not find Boost. If you need it then set BOOST_ROOT and/or BOOST_LIBRARYDIR")
	endif()
endif()

macro(LSLGenerateCPackConfig)
	# CPack configuration
	string(TOUPPER "${PROJECT_NAME}" PROJECT_NAME_UPPER)
	if(NOT PROJECT_DESCRIPTION)
		set(PROJECT_DESCRIPTION "${PROJECT_NAME} ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
	endif()
	set("CPACK_COMPONENT_${PROJECT_NAME_UPPER}_DESCRIPTION" "${PROJECT_DESCRIPTION}" CACHE INTERNAL "CPack Description")

	# top level CMakeLists.txt?
	if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
		# CPack configuration
		set(CPACK_PACKAGE_INSTALL_DIRECTORY "lsl")
		set(CPACK_STRIP_FILES ON)
		set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
		set(CPACK_PACKAGE_NAME lsl)
		set(CPACK_PACKAGE_VENDOR "Labstreaminglayer")
		if(APPLE)
			set(CPACK_GENERATOR "TBZ2")
            if(DEFINED ENV{OSXVER})
                # Configured by Travis-CI for multi-osx builds.
                set(LSL_OS "$ENV{OSXVER}")
            else()
                set(LSL_OS "OSX${lslplatform}")
            endif(DEFINED ENV{OSXVER})
		elseif(WIN32)
			set(CPACK_GENERATOR "7Z") # you can create NSIS packages by calling 'cpack -G NSIS'
			set(CPACK_NSIS_MODIFY_PATH ON)
			set(CPACK_WIX_CMAKE_PACKAGE_REGISTRY ON)
			set(CPACK_WIX_UPGRADE_GUID "ee28a351-3b27-4c2b-8b48-259c87d1b1b4")
			set(CPACK_WIX_PROPERTY_ARPHELPLINK
				"https://labstreaminglayer.readthedocs.io/info/getting_started.html#getting-help")
			set(LSL_OS "Win${lslplatform}")
		elseif(UNIX)
			set(CPACK_GENERATOR DEB)
			set(CPACK_SET_DESTDIR 1)
			set(CPACK_INSTALL_PREFIX "/usr")
			set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Tristan Stenner <ttstenner@gmail.com>")
			set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS 1)
			set(CPACK_DEB_COMPONENT_INSTALL ON)
			set(CPACK_DEBIAN_PACKAGE_PRIORITY optional)

			# include distribution name (e.g. trusty or xenial) in the file name
			find_program(LSB_RELEASE lsb_release)
			execute_process(COMMAND ${LSB_RELEASE} -cs
				OUTPUT_VARIABLE LSB_RELEASE_CODENAME
				OUTPUT_STRIP_TRAILING_WHITESPACE
			)
			set(CPACK_DEBIAN_PACKAGE_RELEASE ${LSB_RELEASE_CODENAME})
			set(LSL_OS "Linux${lslplatform}-${LSB_RELEASE_CODENAME}")
		endif()
		get_cmake_property(CPACK_COMPONENTS_ALL COMPONENTS)
		foreach(component ${CPACK_COMPONENTS_ALL})
			string(TOUPPER ${component} COMPONENT)
			message(STATUS "Setting packages name for ${COMPONENT}")
			set(LSL_CPACK_FILENAME "${component}-${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-${LSL_OS}")
			message(STATUS "File name: ${LSL_CPACK_FILENAME}")
			get_property(LSLDEPENDS GLOBAL PROPERTY "LSLDEPENDS_${component}")
			if(LSLDEPENDS)
				list(REMOVE_DUPLICATES LSLDEPENDS)
				set("CPACK_COMPONENT_${COMPONENT}_DEPENDS" ${LSLDEPENDS})
			endif()

			set("CPACK_DEBIAN_${COMPONENT}_FILE_NAME" "${LSL_CPACK_FILENAME}.deb")
			set("CPACK_ARCHIVE_${COMPONENT}_FILE_NAME" ${LSL_CPACK_FILENAME})
			#set(CPACK_DEBIAN_${component}_FILE_NAME "${FILENAME}.deb")
		endforeach()

		message(STATUS "Installing Components: ${CPACK_COMPONENTS_ALL}")
		include(CPack)
	endif()
endmacro()
