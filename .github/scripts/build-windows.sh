#!/bin/sh -xe

mkdir build
cd build

if [ "$1" = "release" ]; then
    if [ "$2" = "openmp" ]; then
        ENABLE_OPENMP="ON"
    else
        ENABLE_OPENMP="OFF"
    fi
    BUILD_TYPE=RelWithDebInfo
    cmake \
        -G "Visual Studio 16 2019" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DENABLE_OPENMP="${ENABLE_OPENMP}" \
        -DENABLE_LTO=ON \
        -DCMAKE_GENERATOR_PLATFORM="Win32" \
        ..
else
    BUILD_TYPE=Debug
    cmake \
        -G "Visual Studio 16 2019" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DENABLE_OPENMP="ON" \
        -DCMAKE_GENERATOR_PLATFORM="Win32" \
        ..
fi

cmake --build . --config "${BUILD_TYPE}" -- -maxcpucount

bin/$BUILD_TYPE/solvespace-testsuite.exe

if [ "$2" = "openmp" ]; then
    mv bin/$BUILD_TYPE/solvespace.exe bin/$BUILD_TYPE/solvespace-openmp.exe
fi
