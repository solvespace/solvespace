#!/bin/sh -xe

mkdir build
cd build
cmake \
  -DCMAKE_BUILD_TYPE="Debug" \
  -DENABLE_OPENMP="ON" \
  -DENABLE_SANITIZERS="ON" \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  ..
make -j$(nproc) VERBOSE=1
make test_solvespace
