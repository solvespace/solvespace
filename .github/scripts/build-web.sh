#!/usr/bin/env bash

if [ "$1" = "release" ]; then
    BUILD_TYPE="Release"
    ENABLE_LTO="ON"
else
    BUILD_TYPE="Debug"
    ENABLE_LTO="OFF"
fi

cd ..
git clone https://github.com/emscripten-core/emsdk.git --depth 1
cd emsdk
./emsdk install latest
./emsdk activate latest
cd ../solvespace
source ../emsdk/emsdk_env.sh
mkdir build
cd build
emcmake cmake .. \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DENABLE_CLI="OFF" \
  -DENABLE_TESTS="OFF" \
  -DENABLE_COVERAGE="OFF" \
  -DENABLE_OPENMP="OFF" \
  -DENABLE_LTO="ON"
cmake --build . --config "${BUILD_TYPE}" -j$(nproc) --target solvespace
