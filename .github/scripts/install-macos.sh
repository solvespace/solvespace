#!/bin/sh -xe
set -o pipefail

if [ "$1" = "ci" ]; then
    brew_cache=$(brew --cache)
    brew fetch --bottle-tag=arm64_tahoe libomp >/dev/null
    brew fetch --bottle-tag=sonoma libomp >/dev/null
    armloc=$(find "$brew_cache"/downloads -maxdepth 1 -name "*--libomp--*arm64_tahoe.bottle.tar.gz" -print | tail -n1)
    x64loc=$(find "$brew_cache"/downloads -maxdepth 1 -name "*--libomp--*sonoma.bottle.tar.gz" -print | tail -n1)
    [ -n "$armloc" ] || { echo "Failed to locate arm64 libomp bottle in cache" >&2; exit 1; }
    [ -n "$x64loc" ] || { echo "Failed to locate x86_64 libomp bottle in cache" >&2; exit 1; }
    cp "$armloc" /tmp/libomp-arm64.tar.gz
    mkdir /tmp/libomp-arm64 || true
    tar -xzvf /tmp/libomp-arm64.tar.gz -C /tmp/libomp-arm64
    cp "$x64loc" /tmp/libomp-x86_64.tar.gz
    mkdir /tmp/libomp-x86_64 || true
    tar -xzvf /tmp/libomp-x86_64.tar.gz -C /tmp/libomp-x86_64
else
    brew install libomp
fi

git submodule update --init extlib/cairo extlib/freetype extlib/libdxfrw extlib/libpng extlib/mimalloc extlib/pixman extlib/zlib extlib/eigen
