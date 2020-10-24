#!/bin/sh -xe

mkdir build || true
cd build

OSX_TARGET="10.9"
LLVM_PREFIX=$(brew --prefix llvm@9)
export CC="${LLVM_PREFIX}/bin/clang"
export CXX="${CC}++"
export LDFLAGS="-L${LLVM_PREFIX}/lib -Wl,-rpath,${LLVM_PREFIX}/lib" \
export CFLAGS="-I${LLVM_PREFIX}/include"
export CPPFLAGS="-I${LLVM_PREFIX}/include"

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
