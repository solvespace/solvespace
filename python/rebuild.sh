#!/bin/bash

set -e

function cleanup {
  rm -rf src
}

trap cleanup EXIT

cleanup
cp -r ../src src/
pybind11-stubgen solvespace --no-setup-py --root-module-suffix "" -o .
python3.10 -m build .
python3.10 -m pip install --force-reinstall dist/solvespace-0.0.1-cp310-cp310-macosx_13_0_arm64.whl
/Applications/Blender.app/Contents/Resources/3.3/python/bin/python3.10 -m pip install --force-reinstall dist/solvespace-0.0.1-cp310-cp310-macosx_13_0_arm64.whl
pybind11-stubgen solvespace --no-setup-py --root-module-suffix "" -o "/opt/homebrew/lib/python3.11/site-packages/"
python3.10 tests
