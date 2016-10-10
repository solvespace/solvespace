#!/bin/sh -xe

if echo $TRAVIS_TAG | grep ^v; then BUILD_TYPE=RelWithDebInfo; else BUILD_TYPE=Debug; fi

mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE
make VERBOSE=1
