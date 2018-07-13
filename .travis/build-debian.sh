#!/bin/sh -xe

if echo $TRAVIS_TAG | grep ^v; then BUILD_TYPE=RelWithDebInfo; else BUILD_TYPE=Debug; fi

mkdir build
cd build
# We build without the GUI until Travis updates to an Ubuntu version with GTK 3.16+.
cmake .. \
  -DCMAKE_C_COMPILER=gcc-5 \
  -DCMAKE_CXX_COMPILER=g++-5 \
  -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
  -DENABLE_GUI=OFF \
  -DENABLE_SANITIZERS=ON
make VERBOSE=1
make test_solvespace
