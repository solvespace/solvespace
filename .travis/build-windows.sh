#!/bin/sh -xe

MSBUILD_PATH="c:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\MSBuild\15.0\Bin"
export PATH=$MSBUILD_PATH:$PATH

if echo $TRAVIS_TAG | grep ^v; then BUILD_TYPE=RelWithDebInfo; else BUILD_TYPE=Debug; fi

mkdir build
cd build
cmake .. -G "Visual Studio 15 2017 Win64" -DCMAKE_BUILD_TYPE=$BUILD_TYPE

MSBuild.exe "src/solvespace.vcxproj"
MSBuild.exe "src/solvespace-cli.vcxproj"
MSBuild.exe "test/solvespace-testsuite.vcxproj"

bin/$BUILD_TYPE/solvespace-testsuite.exe
