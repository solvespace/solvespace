#!/bin/sh -xe

mkdir build
cd build
cmake \
  -DCMAKE_BUILD_TYPE="Debug" \
  -DENABLE_OPENMP="ON" \
  -DENABLE_SANITIZERS="ON" \
  ..
make -j$(nproc) VERBOSE=1
make test_solvespace
