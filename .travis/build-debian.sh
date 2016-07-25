#!/bin/sh -xe

if echo $TRAVIS_TAG | grep ^v; then BUILD_TYPE=RelWithDebInfo; else BUILD_TYPE=Debug; fi

mkdir build
cd build
cmake -DCMAKE_C_COMPILER=gcc-5 -DCMAKE_CXX_COMPILER=g++-5 \
  -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DENABLE_COVERAGE=ON ..
make VERBOSE=1
