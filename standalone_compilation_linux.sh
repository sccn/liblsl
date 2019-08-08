#!/bin/bash

# This script builds the liblsl.so for linux machines without bigger quirks
# and no recent CMake version, i.e. ARM boards and PCs with old distributions.
# For development, install a recent CMake version, either via pip
# (pip install cmake) or as binary download from cmake.org

set -x
LSLGITREVISION="`git describe --tags HEAD`"
${CXX:-g++} -fPIC -fvisibility=hidden -O2 ${CFLAGS} -Ilslboost \
	-DBOOST_ALL_NO_LIB -DBOOST_ASIO_SEPARATE_COMPILATION -D BOOST_THREAD_BUILD_DLL \
	-DLSL_LIBRARY_INFO_STR=\"${LSLGITREVISION:-"built from standalone build script"}\" \
	src/*.cpp src/pugixml/pugixml.cpp \
	lslboost/asio_objects.cpp \
	lslboost/libs/atomic/src/lockpool.cpp \
	lslboost/libs/chrono/src/chrono.cpp \
	lslboost/libs/serialization/src/*.cpp \
	lslboost/libs/thread/src/pthread/{once,thread}.cpp \
	-shared -o liblsl.so -lpthread
${CC:-gcc} -O2 ${FLAGS} -Iinclude testing/lslver.c -o lslver -L. -llsl
LD_LIBRARY_PATH=. ./lslver
