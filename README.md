SolveSpace
==========

This repository contains the official repository of [SolveSpace][].

[solvespace]: http://solvespace.com

Installation
------------

### Debian (>=jessie) and Ubuntu (>=trusty)

Binary packages for Ubuntu trusty and later versions are available
in [~whitequark/solvespace PPA][ppa].

[ppa]: https://launchpad.net/~whitequark/+archive/ubuntu/solvespace

### Mac OS X (>=10.6 64-bit)

Binary packages for Mac OS X are available via [GitHub releases][rel].

[rel]: https://github.com/whitequark/solvespace/releases

### Other systems

See below.

Building on Linux
-----------------

### Building for Linux

You will need CMake, libpng, zlib, json-c, fontconfig, gtkmm 2.4, pangomm 1.4,
OpenGL and OpenGL GLU.
On a Debian derivative (e.g. Ubuntu) these can be installed with:

    apt-get install libpng12-dev libjson-c-dev libfontconfig1-dev \
                    libgtkmm-2.4-dev libpangomm-1.4-dev libgl-dev libglu-dev \
                    libglew-dev cmake

After that, build SolveSpace as following:

    mkdir cbuild
    cd cbuild
    cmake ..
    make
    sudo make install

A fully functional port to GTK3 is available, but not recommended
for use due to bugs in this toolkit.

### Building for Windows

You will need CMake, a Windows cross-compiler, and Wine with binfmt support.
On a Debian derivative (e.g. Ubuntu) these can be installed with:

    apt-get install cmake mingw-w64 wine-binfmt

Before building, check out the submodules:

    git submodule update --init

After that, build 32-bit SolveSpace as following:

    mkdir cbuild
    cd cbuild
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-mingw32.cmake ..
    make solvespace

Or, build 64-bit SolveSpace as following:

    mkdir cbuild
    cd cbuild
    cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-mingw64.cmake ..
    make solvespace

The application is built as `cbuild/src/solvespace.exe`.

Space Navigator support will not be available.

Building on Mac OS X
--------------------

You will need XCode tools, CMake and libpng. Assuming you use [homebrew][],
these can be installed with:

    brew install cmake libpng

XCode has to be installed via AppStore; it requires a free Apple ID.

After that, build SolveSpace as following:

    mkdir cbuild
    cd cbuild
    cmake ..
    make

The app bundle is built in `cbuild/src/solvespace.app`.

[homebrew]: http://brew.sh/

Building on Windows
-------------------

You will need [cmake][cmakewin] and Visual C++.

You will also need to check out the git submodules.

After installing them, create a directory `build` in the source tree
and point cmake-gui to the source tree and that directory. Press
"Configure" and "Generate", then open `build\solvespace.sln` with
Visual C++ and build it.

Alternatively it is possible to build SolveSpace using [MinGW][mingw].
Run cmake-gui as described above but after pressing "Configure" select
the "MSYS Makefiles" generator. After that, run `make` in the `build`
directory; make sure that the MinGW compiler is in your `PATH`.

[cmakewin]: http://www.cmake.org/download/#latest
[mingw]: http://www.mingw.org/

License
-------

SolveSpace is distributed under the terms of the [GPL3 license](COPYING.txt).
