#!/bin/sh -xe

mkdir build
cd build || true

if [ "$1" = "release" ]; then
  cmake \
    -DCMAKE_BUILD_TYPE="RelWithDebInfo" \
    -DENABLE_OPENMP="ON" \
    -DENABLE_LTO="ON" \
    ..
else
  cmake \
    -DCMAKE_BUILD_TYPE="Debug" \
    -DENABLE_OPENMP="ON" \
    -DENABLE_SANITIZERS="ON" \
    ..
fi

make -j$(nproc) VERBOSE=1
make test_solvespace