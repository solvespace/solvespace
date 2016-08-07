#!/bin/sh -xe

if echo $TRAVIS_TAG | grep ^v; then BUILD_TYPE=RelWithDebInfo; else BUILD_TYPE=Debug; fi

mkdir build
cd build
cmake .. -DCMAKE_C_COMPILER=clang-3.9 -DCMAKE_CXX_COMPILER=clang++-3.9 \
  -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
  -DENABLE_SANITIZERS=ON
make VERBOSE=1
