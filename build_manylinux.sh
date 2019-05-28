#!/bin/bash
# run as
# root=$(git rev-parse --show-superproject-working-tree); [ $root ] || root=$(git rev-parse --show-toplevel)
# docker run --rm -v $root:$root -w $PWD tktechdocker/manylinux:gcc8.3.0 bash build_manylinux.sh

set -e -x

# Install system packages required to build liblsl
PATH=/opt/python/cp37-cp37m/bin:~/.local/bin/:$PATH
python -m pip install --user --only-binary=:all: cmake ninja

mkdir -p build_manylinux && cd build_manylinux

cmake -DCMAKE_C_COMPILER=/usr/local/gcc-8.3.0/bin/gcc-8.3.0 -DCMAKE_CXX_COMPILER=/usr/local/gcc-8.3.0/bin/g++-8.3.0 -DCMAKE_SHARED_LINKER_FLAGS="-static-libstdc++" -DLSL_NO_FANCY_LIBNAME=1 -DLSL_UNIXFOLDERS=1 -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=1 -DCMAKE_POLICY_DEFAULT_CMP0069=NEW -DLSL_SLIM_ARCHIVE=1 -G Ninja ..
ninja install -v
