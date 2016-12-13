SolveSpace
==========

This repository contains the source code of [SolveSpace][], a parametric
2d/3d CAD.

[solvespace]: http://solvespace.com

Installation
------------

### Mac OS X (>=10.6 64-bit), Debian (>=jessie) and Ubuntu (>=trusty)

Binary packages for Mac OS X and Debian derivatives are available
via [GitHub releases][rel].

[rel]: https://github.com/solvespace/solvespace/releases

### Other systems

See below.

Building on Linux
-----------------

### Building for Linux

You will need CMake, libpng, zlib, json-c, fontconfig, freetype, gtkmm 2.4,
pangomm 1.4, OpenGL, OpenGL GLU and OpenGL GLEW, and optionally, the Space Navigator
client library.
On a Debian derivative (e.g. Ubuntu) these can be installed with:

    apt-get install libpng12-dev libjson-c-dev libfreetype6-dev \
                    libfontconfig1-dev libgtkmm-2.4-dev libpangomm-1.4-dev \
                    libgl-dev libglu-dev libglew-dev libspnav-dev cmake

Before building, check out the necessary submodules:

    git submodule update --init extlib/libdxfrw

After that, build SolveSpace as following:

    mkdir build
    cd build
    cmake ..
    make
    sudo make install

A fully functional port to GTK3 is available, but not recommended
for use due to bugs in this toolkit.

### Building for Windows

You will need CMake, a Windows cross-compiler, and Wine with binfmt support.
On a Debian derivative (e.g. Ubuntu) these can be installed with:

    apt-get install cmake mingw-w64 wine-binfmt

Before building, check out the necessary submodules:

    git submodule update --init

After that, build 32-bit SolveSpace as following:

    mkdir build
    cd build
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-mingw32.cmake ..
    make solvespace

Or, build 64-bit SolveSpace as following:

    mkdir build
    cd build
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-mingw64.cmake ..
    make solvespace

The application is built as `build/src/solvespace.exe`.

Space Navigator support will not be available.

Building on Mac OS X
--------------------

You will need XCode tools, CMake, libpng and Freetype. Assuming you use
[homebrew][], these can be installed with:

    brew install cmake libpng freetype

XCode has to be installed via AppStore; it requires a free Apple ID.

Before building, check out the necessary submodules:

    git submodule update --init extlib/libdxfrw

After that, build SolveSpace as following:

    mkdir build
    cd build
    cmake ..
    make

The app bundle is built in `build/src/solvespace.app`.

[homebrew]: http://brew.sh/

Building on Windows
-------------------

You will need [cmake][cmakewin] and Visual C++.

### GUI build

Check out the git submodules. Create a directory `build` in
the source tree and point cmake-gui to the source tree and that directory.
Press "Configure" and "Generate", then open `build\solvespace.sln` with
Visual C++ and build it.

### Command-line build

First, ensure that git and cl (the Visual C++ compiler driver) are in your
`%PATH%`; the latter is usually done by invoking `vcvarsall.bat` from your
Visual Studio install. Then, run the following in cmd or PowerShell:

    git submodule update --init
    mkdir build
    cd build
    cmake .. -G "NMake Makefiles"
    nmake

### MSVC build

It is also possible to build SolveSpace using [MinGW][mingw], though
Space Navigator support will be disabled.

First, ensure that git and gcc are in your `$PATH`. Then, run the following
in bash:

    git submodule update --init
    mkdir build
    cd build
    cmake ..
    make

[cmakewin]: http://www.cmake.org/download/#latest
[mingw]: http://www.mingw.org/

License
-------

SolveSpace is distributed under the terms of the [GPL3 license](COPYING.txt).
