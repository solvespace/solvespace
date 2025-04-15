#!/usr/bin/env bash

cd ..
git clone https://github.com/emscripten-core/emsdk.git --depth 1
cd emsdk
./emsdk install latest
./emsdk activate latest
cd ../solvespace
source ../emsdk/emsdk_env.sh
mkdir -p build-wasmlib
cd build-wasmlib
emcmake cmake .. \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DENABLE_GUI="OFF" \
  -DENABLE_CLI="OFF" \
  -DENABLE_TESTS="OFF" \
  -DENABLE_COVERAGE="OFF" \
  -DENABLE_OPENMP="OFF" \
  -DFORCE_VENDORED_Eigen3="ON" \
  -DENABLE_LTO="ON"
cmake --build . -j$(nproc) --target slvs-wasm
