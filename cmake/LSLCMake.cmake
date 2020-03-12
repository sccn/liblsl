# Common functions and settings for LSL

# Dummy function that should be called after find_package(LSL)
# Does nothing at the moment, but the entire code below should be within this
# function so it's not executed by accident
macro(LSLAPP_Setup_Boilerplate)
endmacro()

message(STATUS "Included LSL CMake helpers, rev. 12, ${CMAKE_CURRENT_LIST_DIR}")
option(LSL_DEPLOYAPPLIBS "Copy library dependencies (at the moment Qt + liblsl) to the installation dir" ON)

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

# Generate folders for IDE targets (e.g., VisualStudio solutions)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)


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

# installLSLApp: adds the specified target to the install list and
# add some quality-of-life improvements for Qt executables
# After the target, additional libraries to install alongside the target can be
# specified, e.g. 	installLSLApp(FooApp libXY libZ)
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

	# Set runtime path, i.e. where shared libs are searched relative to the exe
	set(LIBDIRGENEXPR "../$<IF:$<BOOL:${LSL_UNIXFOLDERS}>,lib/,LSL/lib/>")
	if(APPLE)
		set_property(TARGET ${target} APPEND
			PROPERTY INSTALL_RPATH "@executable_path/;@executable_path/${LIBDIRGENEXPR}")
	elseif(UNIX)
		set_property(TARGET ${target}
			PROPERTY INSTALL_RPATH "\$ORIGIN:\$ORIGIN/${LIBDIRGENEXPR}")
	endif()

	if(LSL_UNIXFOLDERS)
		include(GNUInstallDirs)
		set(lsldir "\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
	else()
		set(CMAKE_INSTALL_BINDIR ${PROJECT_NAME})
		set(CMAKE_INSTALL_LIBDIR ${PROJECT_NAME})
		set(lsldir "\${CMAKE_INSTALL_PREFIX}/LSL")
	endif()
	
	# add start menu shortcut if supported by installer
	set_property(INSTALL "${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:${target}>" PROPERTY
		CPACK_START_MENU_SHORTCUTS "${target}")

	# install additional library dependencies supplied after the target argument
	foreach(libdependency ${ARGN})
		if(NOT TARGET ${libdependency})
			message(FATAL_ERROR "Additional arguments to installLSLApp must be library targets, ${libdependency} isn't.")
		endif()
		install(CODE "file(INSTALL $<TARGET_FILE:${libdependency}> DESTINATION \${CMAKE_INSTALL_PREFIX}/$<IF:$<PLATFORM_ID:Windows>,${CMAKE_INSTALL_BINDIR},${CMAKE_INSTALL_LIBDIR}>)")
	endforeach()

	set_property(GLOBAL APPEND PROPERTY
		"LSLDEPENDS_${PROJECT_NAME}" liblsl)
	install(TARGETS ${target} COMPONENT ${PROJECT_NAME}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR})
	# skip copying libraries if disabled or on Linux
	if(NOT LSL_DEPLOYAPPLIBS OR UNIX AND NOT APPLE)
		return()
	endif()

	# skip exe deployment for libraries
	get_target_property(target_type ${target} TYPE)
	if(NOT target_type STREQUAL "EXECUTABLE")
		return()
	endif()
	
	# Some Windows installers have problems with several components having the same file,
	# so libs shared between targets are copied into the SHAREDLIBCOMPONENT component if set
	if(NOT SHAREDLIBCOMPONENT)
		set(SHAREDLIBCOMPONENT ${PROJECT_NAME})
	endif()

	# For MacOS we need to know if the installed target will be a .app bundle...
	if(APPLE)
		get_target_property(target_is_bundle ${target} MACOSX_BUNDLE)
		set(APPLE_APP_PATH "\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${target}.app")
	endif(APPLE)
	
	# Copy lsl library. Logic is a bit complicated.
	# If we are not building liblsl, and the (app) target is not using system liblsl
	# (LSL_UNIXFOLDERS is maybe a poor proxy for use of system liblsl...),
	# then the application needs to have liblsl in an expected location.
	if(NOT TARGET liblsl AND NOT LSL_UNIXFOLDERS)
		if(APPLE AND target_is_bundle)
			# Copy the dylib into the bundle
			install(FILES $<TARGET_FILE:LSL::lsl>
				DESTINATION ${CMAKE_INSTALL_BINDIR}/${target}.app/Contents/MacOS/
				COMPONENT ${SHAREDLIBCOMPONENT})
		else()
			# Copy the dll/dylib/so next to the executable binary.
			install(FILES $<TARGET_FILE:LSL::lsl>
				DESTINATION ${CMAKE_INSTALL_BINDIR}
				COMPONENT ${SHAREDLIBCOMPONENT})
		endif(APPLE AND target_is_bundle)
	endif(NOT TARGET liblsl AND NOT LSL_UNIXFOLDERS)
	# Mac bundles need further fixup (mostly for 3rd party libs)
	# fixup_bundle appears to be broken for Qt apps. Use only for non-Qt.
	if(APPLE AND target_is_bundle AND NOT qtapp)
		install(CODE
			"
				get_filename_component(LIBDIR $<TARGET_FILE:LSL::lsl> DIRECTORY)
				message(STATUS \${LIBDIR})
				include(BundleUtilities)
				set(BU_CHMOD_BUNDLE_ITEMS ON)
				fixup_bundle(${APPLE_APP_PATH} \"\" \"\${LIBDIR}\")
			"
			COMPONENT ${PROJECT_NAME}
		)
	endif()

	# skip the rest if qt doesn't need to be deployed
	if(NOT qtapp)
		return()
	endif()

	cmake_minimum_required(VERSION 3.15) # generator expressions in install(CODE)
	if(WIN32)
		findQtInstallationTool("windeployqt")
		install(CODE "
			message (STATUS \"Running windeployqt on $<TARGET_FILE:${target}>\")
			execute_process(
				COMMAND \"${QT_DEPLOYQT_EXECUTABLE}\" --no-translations
				--no-system-d3d-compiler --no-opengl-sw
				--no-compiler-runtime --dry-run --list mapping
				\"$<TARGET_FILE:${target}>\"
				OUTPUT_VARIABLE output
				OUTPUT_STRIP_TRAILING_WHITESPACE
			)
			file(TO_CMAKE_PATH \"\${output}\" output) # convert slashes
			separate_arguments(_files WINDOWS_COMMAND \${output})
			while(_files)
				list(POP_FRONT _files _src)
				list(POP_FRONT _files _dest)
				get_filename_component(_dest \${_dest} DIRECTORY RELATIVE)
				file(COPY \${_src} DESTINATION \${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/\${_dest})
			endwhile()"
			COMPONENT ${SHAREDLIBCOMPONENT})
	elseif(APPLE)
		# It should be enough to call fixup_bundle (see above),
		# but this fails to install qt plugins (cocoa).
		# Use macdeployqt instead. This is bad at grabbing lsl dylib, so we already did it above.
		findQtInstallationTool("macdeployqt")
		install(CODE "
			message(STATUS \"Running Qt Deploy Tool for $<TARGET_FILE:${target}>...\")
			execute_process(COMMAND
				\"${QT_DEPLOYQT_EXECUTABLE}\"
				\"${APPLE_APP_PATH}\"
				-verbose=1 -always-overwrite
				\$<\$<STREQUAL:\${CMAKE_INSTALL_CONFIG_NAME},\"Debug\">:--use-debug-libs>
				COMMAND_ECHO STDOUT
				OUTPUT_VARIABLE DEPLOYOUTPUT
				ERROR_VARIABLE DEPLOYOUTPUT
				RESULT_VARIABLE DEPLOYSTATUS
			)
			if(\${DEPLOYSTATUS})
				message(WARNING \"\${DEPLOYOUTPUT}\")
			endif()
		")
	endif()
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
		message(WARNING "Qt deploy tool wasn't found, installing ${PROJECT_NAME} will fail!")
		return()
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
		set(PROJECT_DESCRIPTION "${PROJECT_NAME} ${PROJECT_VERSION}")
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
			set(LSL_CPACK_DEFAULT_GEN TBZ2)
			if(DEFINED ENV{OSXVER})
				# Configured by Travis-CI for multi-osx builds.
				set(LSL_OS "$ENV{OSXVER}")
			else()
				set(LSL_OS "OSX${lslplatform}")
			endif(DEFINED ENV{OSXVER})
		elseif(WIN32)
			set(LSL_CPACK_DEFAULT_GEN ZIP)
			set(CPACK_NSIS_MODIFY_PATH ON)
			set(CPACK_WIX_CMAKE_PACKAGE_REGISTRY ON)
			set(CPACK_WIX_UPGRADE_GUID "ee28a351-3b27-4c2b-8b48-259c87d1b1b4")
			set(CPACK_WIX_PROPERTY_ARPHELPLINK
				"https://labstreaminglayer.readthedocs.io/info/getting_started.html#getting-help")
			set(LSL_OS "Win${lslplatform}")
		elseif(UNIX)
			set(LSL_CPACK_DEFAULT_GEN DEB)
			set(CPACK_SET_DESTDIR 1)
			set(CPACK_INSTALL_PREFIX "/usr")
			set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Tristan Stenner <ttstenner@gmail.com>")
			set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS ON)
			set(CPACK_DEB_COMPONENT_INSTALL ON)
			set(CPACK_DEBIAN_PACKAGE_PRIORITY optional)
			set(CPACK_DEBIAN_LIBLSL_PACKAGE_SHLIBDEPS ON)
			set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)

			# include distribution name (e.g. trusty or xenial) in the file name
			find_program(LSB_RELEASE lsb_release)
			execute_process(COMMAND ${LSB_RELEASE} -cs
				OUTPUT_VARIABLE LSB_RELEASE_CODENAME
				OUTPUT_STRIP_TRAILING_WHITESPACE
				RESULT_VARIABLE LSB_RELEASE_RESULT
			)
			if(LSB_RELEASE_RESULT)
				set(CPACK_DEBIAN_PACKAGE_RELEASE ${LSB_RELEASE_CODENAME})
			endif()
			set(LSL_OS "Linux${lslplatform}-${LSB_RELEASE_CODENAME}")
		endif()
		set(CPACK_GENERATOR ${LSL_CPACK_DEFAULT_GEN} CACHE STRING "CPack pkg type(s) to generate")
		get_cmake_property(LSL_COMPONENTS COMPONENTS)
		foreach(component ${LSL_COMPONENTS})
			string(TOUPPER ${component} COMPONENT)
			set(LSL_CPACK_FILENAME "${component}-${PROJECT_VERSION}-${LSL_OS}")
			get_property(LSLDEPENDS GLOBAL PROPERTY "LSLDEPENDS_${component}")
			if(LSLDEPENDS)
				list(REMOVE_DUPLICATES LSLDEPENDS)
				# remove dependencies we don't package ourselves
				set(MISSING ${LSLDEPENDS})
				list(REMOVE_ITEM MISSING ${LSL_COMPONENTS})
				if(MISSING)
					list(REMOVE_ITEM LSLDEPENDS ${MISSING})
				endif()
				set("CPACK_COMPONENT_${COMPONENT}_DEPENDS" ${LSLDEPENDS})
			endif()

			set("CPACK_DEBIAN_${COMPONENT}_PACKAGE_NAME" ${component})
			set("CPACK_DEBIAN_${COMPONENT}_FILE_NAME" "${LSL_CPACK_FILENAME}.deb")
			set("CPACK_ARCHIVE_${COMPONENT}_FILE_NAME" ${LSL_CPACK_FILENAME})
		endforeach()

		message(STATUS "Installing Components: ${LSL_COMPONENTS}")
		include(CPack)
	endif()
endmacro()
