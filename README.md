SolveSpace
==========

This repository contains the source code of [SolveSpace][], a parametric
2d/3d CAD.

[solvespace]: http://solvespace.com

Installation
------------

### Mac OS X (>=10.6 64-bit), Windows (>=XP 32-bit)

Binary packages for Mac OS X and Windows are available via
[GitHub releases][rel].

[rel]: https://github.com/solvespace/solvespace/releases

### Other systems

See below.

Building on Linux
-----------------

### Building for Linux

You will need CMake, libpng, zlib, json-c, fontconfig, freetype, gtkmm 2.4,
pangomm 1.4, OpenGL and OpenGL GLU. To build tests, you will need cairo.
On a Debian derivative (e.g. Ubuntu) these can be installed with:

    apt-get install libpng-dev libjson-c-dev libfreetype6-dev \
                    libfontconfig1-dev libgtkmm-2.4-dev libpangomm-1.4-dev \
                    libcairo2-dev libgl-dev libglu-dev cmake

Before building, check out the necessary submodules:

    git submodule update --init extlib/libdxfrw

After that, build SolveSpace as following:

    mkdir build
    cd build
    cmake .. -DENABLE_TESTS=OFF
    make
    sudo make install

A fully functional port to GTK3 is available, but not recommended
for use due to bugs in this toolkit.

### Building for Windows

You will need CMake and a Windows cross-compiler.
On a Debian derivative (e.g. Ubuntu) these can be installed with:

    apt-get install cmake mingw-w64

Before building, check out the necessary submodules:

    git submodule update --init

After that, build 32-bit SolveSpace as following:

    mkdir build
    cd build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-mingw32.cmake \
             -DENABLE_TESTS=OFF
    make

Or, build 64-bit SolveSpace as following:

    mkdir build
    cd build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-mingw64.cmake \
             -DENABLE_TESTS=OFF
    make

The application is built as `build/src/solvespace.exe`.

Space Navigator support will not be available.

Building on Mac OS X
--------------------

You will need XCode tools, CMake, libpng and Freetype. To build tests, you
will need cairo. Assuming you use
[homebrew][], these can be installed with:

    brew install cmake libpng freetype cairo

XCode has to be installed via AppStore; it requires a free Apple ID.

Before building, check out the necessary submodules:

    git submodule update --init extlib/libdxfrw

After that, build SolveSpace as following:

    mkdir build
    cd build
    cmake .. -DENABLE_TESTS=OFF -DCMAKE_BUILD_TYPE=Debug
    make

The app bundle is built in `build/src/solvespace.app`.

[homebrew]: http://brew.sh/

### Debugging in Mac OS X

Start the debug build of solvespace in lldb:

    lldb build/src/solvespace.app/Contents/MacOS/solvespace
    (lldb) run
    
    (reporoduce issue, to invoke exception - or place a breakpoint)
    
    (lldb) bt all
    (lldb) frame select [n]
    (lldb) frame variable

Building on Windows
-------------------

You will need [git][gitwin],  [cmake][cmakewin] and Visual C++.

### Building with Visual Studio IDE

Check out the git submodules. Create a directory `build` in
the source tree and point cmake-gui to the source tree and that directory.
Press "Configure" and "Generate", then open `build\solvespace.sln` with
Visual C++ and build it.

### Building with Visual Studio in a command prompt

First, ensure that git and cl (the Visual C++ compiler driver) are in your
`%PATH%`; the latter is usually done by invoking `vcvarsall.bat` from your
Visual Studio install. Then, run the following in cmd or PowerShell:

    git submodule update --init
    mkdir build
    cd build
    cmake .. -G "NMake Makefiles" -DENABLE_TESTS=OFF
    nmake

### Building with MinGW

It is also possible to build SolveSpace using [MinGW][mingw], though
Space Navigator support will be disabled.

First, ensure that git and gcc are in your `$PATH`. Then, run the following
in bash:

    git submodule update --init
    mkdir build
    cd build
    cmake .. -DENABLE_TESTS=OFF
    make

[gitwin]: https://git-scm.com/download/win
[cmakewin]: http://www.cmake.org/download/#latest
[mingw]: http://www.mingw.org/

License
-------

SolveSpace is distributed under the terms of the [GPL3 license](COPYING.txt).
