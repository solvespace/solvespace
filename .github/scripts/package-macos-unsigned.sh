#!/bin/bash -xe

# The x86_64 slice is built without OpenMP (Homebrew no longer ships an
# x86_64 libomp bottle), so there is nothing to lipo. Only the arm64 slice
# links libomp, so ship the arm64-only dylib.
cp build-arm64/bin/SolveSpace.app/Contents/Resources/libomp.dylib \
   build/bin/SolveSpace.app/Contents/Resources/libomp.dylib

lipo \
    -create \
        build/bin/SolveSpace.app/Contents/MacOS/SolveSpace \
        build-arm64/bin/SolveSpace.app/Contents/MacOS/SolveSpace \
    -output \
        build/bin/SolveSpace.app/Contents/MacOS/SolveSpace

lipo \
    -create \
        build/bin/SolveSpace.app/Contents/MacOS/solvespace-cli \
        build-arm64/bin/SolveSpace.app/Contents/MacOS/solvespace-cli \
    -output \
        build/bin/SolveSpace.app/Contents/MacOS/solvespace-cli

# The bundle was modified by cp and lipo above and arm64 binaries require a
# valid signature to run, so re-sign the bundle ad hoc (no identity).
codesign --force --deep -s - build/bin/SolveSpace.app

hdiutil create -srcfolder build/bin/SolveSpace.app build/bin/SolveSpace.dmg
