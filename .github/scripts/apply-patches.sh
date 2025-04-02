#!/bin/bash
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
REPO_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
PATCHES_DIR="$SCRIPT_DIR/../patches"

# Apply mimalloc CMake version patch
echo "Applying mimalloc CMake version patch..."
patch -p1 -d "$REPO_ROOT/extlib/mimalloc" < "$PATCHES_DIR/mimalloc-cmake-version.patch"