#!/bin/sh -xe

mkdir build || true
cd build

OSX_TARGET="10.9"

if [ "$1" = "release" ]; then
    BUILD_TYPE=RelWithDebInfo
    cmake \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${OSX_TARGET}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DENABLE_OPENMP="ON" \
        -DENABLE_LTO="ON" \
        ..
else
    BUILD_TYPE=Debug
    cmake \
        -DCMAKE_OSX_DEPLOYMENT_TARGET="${OSX_TARGET}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DENABLE_OPENMP="ON" \
        -DENABLE_SANITIZERS="ON" \
        ..
fi

cmake --build . --config "${BUILD_TYPE}" -- -j$(nproc)
make -j$(nproc) test_solvespace
