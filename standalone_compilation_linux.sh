#!/bin/sh

# This script builds the liblsl.so for linux machines without bigger quirks
# and no recent CMake version, i.e. ARM boards and PCs with old distributions.
# For development, install a recent CMake version, either via pip
# (pip install cmake) or as binary download from cmake.org

set -x
# Try to read LSLGITREVISION from git if the variable isn't set
echo ${LSLGITREVISION:="$(git describe --tags HEAD)"}
${CXX:-g++} -fPIC -fvisibility=hidden -O2 ${CFLAGS} -Ilslboost \
	-DBOOST_ALL_NO_LIB \
	-DLSL_LIBRARY_INFO_STR=\"${LSLGITREVISION:-"built from standalone build script"}\" \
	src/*.cpp src/pugixml/pugixml.cpp \
	lslboost/libs/atomic/src/lockpool.cpp \
	lslboost/libs/chrono/src/chrono.cpp \
	lslboost/libs/serialization/src/*.cpp \
	lslboost/libs/thread/src/pthread/once.cpp \
	lslboost/libs/thread/src/pthread/thread.cpp \
	${LDFLAGS} \
	-shared -o liblsl.so -lpthread -lrt
${CC:-gcc} -O2 ${CFLAGS} -Iinclude testing/lslver.c -o lslver -L. -llsl
LD_LIBRARY_PATH=. ./lslver
