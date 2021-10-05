#!/bin/sh

# This script builds the liblsl.so for linux machines without bigger quirks
# and no recent CMake version, i.e. ARM boards and PCs with old distributions.
# For development, install a recent CMake version, either via pip
# (pip install cmake) or as binary download from cmake.org

set -e -x
# Try to read LSLGITREVISION from git if the variable isn't set
echo ${LSLGITREVISION:="$(git describe --tags HEAD)"}
${CXX:-g++} -fPIC -fvisibility=hidden -O2 ${CFLAGS} ${CXXFLAGS} -Ilslboost \
	-DBOOST_ALL_NO_LIB \
	-DASIO_NO_DEPRECATED \
	-DLOGURU_DEBUG_LOGGING=0 \
	-DLSL_LIBRARY_INFO_STR=\"${LSLGITREVISION:-"built from standalone build script"}\" \
	src/*.cpp src/util/*.cpp \
	thirdparty/pugixml/pugixml.cpp -Ithirdparty/pugixml \
	thirdparty/loguru/loguru.cpp -Ithirdparty/loguru \
	-Ithirdparty/asio \
	lslboost/serialization_objects.cpp \
	${LDFLAGS} \
	-shared -o liblsl.so -lpthread -lrt -ldl
${CC:-gcc} -O2 ${CFLAGS} -Iinclude testing/lslver.c -o lslver -L. -llsl
LD_LIBRARY_PATH=. ./lslver
