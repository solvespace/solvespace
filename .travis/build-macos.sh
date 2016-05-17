#!/bin/sh -xe

if echo $TRAVIS_TAG | grep ^v; then BUILD_TYPE=RelWithDebInfo; else BUILD_TYPE=Debug; fi

mkdir build
cd build
cmake -DCMAKE_OSX_DEPLOYMENT_TARGET=10.7 -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..
if ! make VERBOSE=1; then
  echo "Sigh, transient build failure. Retrying..."
  if ! make VERBOSE=1; then
    echo "Okay, this is probably an actual bug."
    exit 1
  fi
fi
