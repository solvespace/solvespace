#!/bin/sh -xe

MSBUILD_PATH="c:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin"
export PATH=$MSBUILD_PATH:$PATH

mkdir build
cd build

if [ "$1" == "release" ]; then
    BUILD_TYPE=RelWithDebInfo
    cmake \
        -G "Visual Studio 15 2017 Win64" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" .. \
        -DENABLE_OPENMP=ON \
        -DENABLE_LTO=ON
else
    BUILD_TYPE=Debug
    cmake \
        -G "Visual Studio 15 2017 Win64" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" ..
fi

cmake --build . --config "${BUILD_TYPE}" -- -maxcpucount

bin/$BUILD_TYPE/solvespace-testsuite.exe
