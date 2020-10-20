#!/bin/sh -xe

if echo $TRAVIS_TAG | grep ^v; then
    BUILD_TYPE=RelWithDebInfo
else
    BUILD_TYPE=Debug
fi

mkdir build || true
cd build

LLVM_PREFIX=$(brew --prefix llvm@9)
export CC="${LLVM_PREFIX}/bin/clang"
export CXX="${CC}++"
export LDFLAGS="-L${LLVM_PREFIX}/lib -Wl,-rpath,${LLVM_PREFIX}/lib" \
export CFLAGS="-I${LLVM_PREFIX}/include"
export CPPFLAGS="-I${LLVM_PREFIX}/include"

cmake \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.9 \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE .. \
    -DENABLE_OPENMP=ON

cmake --build . --config $BUILD_TYPE -- -j$(nproc)
make -j$(nproc) test_solvespace
