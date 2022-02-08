#!/bin/sh -xe

if [ "$1" = "ci" ]; then
    armloc=$(brew fetch --bottle-tag=arm64_big_sur libomp | grep -i downloaded | grep tar.gz | cut -f2 -d:)
    x64loc=$(brew fetch --bottle-tag=big_sur libomp | grep -i downloaded | grep tar.gz | cut -f2 -d:)
    cp $armloc /tmp/libomp-arm64.tar.gz
    mkdir /tmp/libomp-arm64 || true
    tar -xzvf /tmp/libomp-arm64.tar.gz -C /tmp/libomp-arm64
    cp $x64loc /tmp/libomp-x86_64.tar.gz
    mkdir /tmp/libomp-x86_64 || true
    tar -xzvf /tmp/libomp-x86_64.tar.gz -C /tmp/libomp-x86_64
else
    brew install libomp
fi

git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen
