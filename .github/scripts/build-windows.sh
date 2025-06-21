#!/bin/sh -xe

mkdir build
cd build

if [ "$1" = "release" ]; then
    if [ "$2" = "openmp" ]; then
        ENABLE_OPENMP="ON"
    else
        ENABLE_OPENMP="OFF"
    fi

    if [ "$3" = "x64" ]; then
        PLATFORM="$3"
    else
        PLATFORM="Win32"
    fi

    BUILD_TYPE=RelWithDebInfo
    cmake \
        -G "Visual Studio 17 2022" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DENABLE_OPENMP="${ENABLE_OPENMP}" \
        -DENABLE_LTO=ON \
        -DCMAKE_GENERATOR_PLATFORM="${PLATFORM}" \
        ..
else
    BUILD_TYPE=Debug
    cmake \
        -G "Visual Studio 17 2022" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DENABLE_OPENMP="ON" \
        -DCMAKE_GENERATOR_PLATFORM="Win32" \
        ..
fi

cmake --build . --config "${BUILD_TYPE}" -- -maxcpucount

bin/$BUILD_TYPE/solvespace-testsuite.exe

if [ "$3" = "x64" ]; then
	if [ "$2" != "openmp" ]; then
		mv bin/$BUILD_TYPE/solvespace.exe bin/$BUILD_TYPE/solvespace_single_core_x64.exe
	else
		mv bin/$BUILD_TYPE/solvespace.exe bin/$BUILD_TYPE/solvespace_x64.exe
	fi
else
	if [ "$2" != "openmp" ]; then
		mv bin/$BUILD_TYPE/solvespace.exe bin/$BUILD_TYPE/solvespace_single_core.exe
	fi
fi
