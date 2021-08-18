#!/bin/sh -xe

ENABLE_SANITIZERS="OFF"
if [ "$1" = "release" ]; then
    BUILD_TYPE="RelWithDebInfo"
    ENABLE_LTO="ON"
else
    BUILD_TYPE="Debug"
    ENABLE_LTO="OFF"
fi

# this is an option for our Github CI only, since it doesn't have a macos arm64 image yet
CMAKE_GENERATOR="Unix Makefiles"
CMAKE_PREFIX_PATH=""
if [ "$2" = "arm64" ]; then
    OSX_ARCHITECTURE="arm64"
    CMAKE_PREFIX_PATH=$(find /tmp/libomp-arm64/libomp -depth 1)
    git apply cmake/libpng-macos-arm64.patch || echo "Could not apply patch, probably already patched..."
    mkdir build-arm64 || true
    cd build-arm64
elif [ "$2" = "x86_64" ]; then
    OSX_ARCHITECTURE="x86_64"
    CMAKE_PREFIX_PATH=$(find /tmp/libomp-x86_64/libomp -depth 1)
    mkdir build || true
    cd build
else
    mkdir build || true
    cd build
fi

if [ "$3" = "xcode" ]; then
    CMAKE_GENERATOR="Xcode"
fi

cmake \
    -G "${CMAKE_GENERATOR}" \
    -D CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
    -D CMAKE_OSX_ARCHITECTURES="${OSX_ARCHITECTURE}" \
    -D CMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -D ENABLE_OPENMP="ON" \
    -D ENABLE_SANITIZERS="${ENABLE_SANITIZERS}" \
    -D ENABLE_LTO="${ENABLE_LTO}" \
    ..

if [ "$3" = "xcode" ]; then
    open solvespace.xcodeproj
else
    cmake --build . --config "${BUILD_TYPE}" -j$(sysctl -n hw.logicalcpu)
    if [ $(uname -m) = "$2" ]; then
        make -j$(sysctl -n hw.logicalcpu) test_solvespace
    fi
fi