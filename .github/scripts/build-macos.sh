#!/bin/sh -xe

if [ "$1" = "release" ]; then
    BUILD_TYPE="RelWithDebInfo"
    ENABLE_SANITIZERS="OFF"
    ENABLE_LTO="ON"
else
    BUILD_TYPE="Debug"
    ENABLE_SANITIZERS="OFF"
    ENABLE_LTO="OFF"
fi

OSX_ARCHITECTURE="x86_64"
if [ "$2" = "silicon" ]; then
    OSX_ARCHITECTURE="arm64"
    mkdir buildsilicon || true
    cd buildsilicon
else
    mkdir build || true
    cd build
fi

cmake \
    -D CMAKE_C_COMPILER="clang" \
    -D CMAKE_CXX_COMPILER="clang++" \
    -D CMAKE_OSX_ARCHITECTURES="${OSX_ARCHITECTURE}" \
    -D CMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -D ENABLE_OPENMP="ON" \
    -D ENABLE_SANITIZERS="${ENABLE_SANITIZERS}" \
    -D ENABLE_LTO="${ENABLE_LTO}" \
    ..

cmake --build . --config "${BUILD_TYPE}" -j $(sysctl -n hw.ncpu)

if [ "$2" != "silicon" ]; then
    make test_solvespace
fi