SolveSpace
==========

This repository contains the source code of [SolveSpace][], a parametric
2d/3d CAD.

[solvespace]: http://solvespace.com

Installation
------------

### macOS (>=10.6 64-bit), Windows (>=Vista 32-bit)

Binary packages for macOS and Windows are available via
[GitHub releases][rel].

[rel]: https://github.com/solvespace/solvespace/releases

### Other systems

See below.

Building on Linux
-----------------

### Building for Linux

You will need the usual build tools, CMake, zlib, libpng, cairo, freetype.
To build the GUI, you will need fontconfig, gtkmm 3.0 (version 3.16 or later), pangomm 1.4,
OpenGL and OpenGL GLU, and optionally, the Space Navigator client library.
On a Debian derivative (e.g. Ubuntu) these can be installed with:

    apt-get install git build-essential cmake zlib1g-dev libpng-dev libcairo2-dev libfreetype6-dev
    apt-get install libjson-c-dev libfontconfig1-dev libgtkmm-3.0-dev libpangomm-1.4-dev \
                    libgl-dev libglu-dev libspnav-dev

Before building, check out the project and the necessary submodules:

    git clone https://github.com/solvespace/solvespace
    cd solvespace
    git submodule update --init extlib/libdxfrw

After that, build SolveSpace as following:

    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make
    sudo make install

The graphical interface is built as `build/bin/solvespace`, and the command-line interface
is built as `build/bin/solvespace-cli`. It is possible to build only the command-line interface
by passing the `-DENABLE_GUI=OFF` flag to the cmake invocation.

### Building for Windows

You will need the usual build tools, CMake and a Windows cross-compiler.
On a Debian derivative (e.g. Ubuntu) these can be installed with:

    apt-get install git build-essential cmake mingw-w64

Before building, check out the project and the necessary submodules:

    git clone https://github.com/solvespace/solvespace
    cd solvespace
    git submodule update

After that, build 32-bit SolveSpace as following:

    mkdir build
    cd build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-mingw32.cmake \
             -DCMAKE_BUILD_TYPE=Release
    make

Or, build 64-bit SolveSpace as following:

    mkdir build
    cd build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchain-mingw64.cmake \
             -DCMAKE_BUILD_TYPE=Release
    make

The graphical interface is built as `build/bin/solvespace.exe`, and the command-line interface
is built as `build/bin/solvespace-cli.exe`.

Space Navigator support will not be available.

Building on macOS
-----------------

You will need XCode tools, CMake, libpng and Freetype. To build tests, you
will need cairo. Assuming you use [homebrew][], these can be installed with:

    brew install git cmake libpng freetype cairo

XCode has to be installed via AppStore or [the Apple website][appledeveloper];
it requires a free Apple ID.

Before building, check out the project and the necessary submodules:

    git clone https://github.com/solvespace/solvespace
    cd solvespace
    git submodule update --init extlib/libdxfrw

After that, build SolveSpace as following:

    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make

Alternatively, generate an XCode project, open it, and build the "Release" scheme:

    mkdir build
    cd build
    cmake .. -G Xcode

The application is built in `build/bin/solvespace.app`, the graphical interface executable
is `build/bin/solvespace.app/Contents/MacOS/solvespace`, and the command-line interface executable
is `build/bin/solvespace.app/Contents/MacOS/solvespace-cli`.

[homebrew]: https://brew.sh/
[appledeveloper]: https://developer.apple.com/download/

Building on OpenBSD
-------------------

You will need git, cmake, libexecinfo, libpng, gtk3mm and pangomm.
These can be installed from the ports tree:

    pkg_add -U git cmake libexecinfo png json-c gtk3mm pangomm

Before building, check out the project and the necessary submodules:

    git clone https://github.com/solvespace/solvespace
    cd solvespace
    git submodule update --init extlib/libdxfrw

After that, build SolveSpace as following:

    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make

Unfortunately, on OpenBSD, the produced executables are not filesystem location independent
and must be installed before use. By default, the graphical interface is installed to
`/usr/local/bin/solvespace`, and the command-line interface is built as
`/usr/local/bin/solvespace-cli`. It is possible to build only the command-line interface
by passing the `-DENABLE_GUI=OFF` flag to the cmake invocation.

Building on Windows
-------------------

You will need [git][gitwin], [cmake][cmakewin] and Visual C++.

### Building with Visual Studio IDE

Check out the git submodules. Create a directory `build` in
the source tree and point cmake-gui to the source tree and that directory.
Press "Configure" and "Generate", then open `build\solvespace.sln` with
Visual C++ and build it.

### Building with Visual Studio in a command prompt

First, ensure that git and cl (the Visual C++ compiler driver) are in your
`%PATH%`; the latter is usually done by invoking `vcvarsall.bat` from your
Visual Studio install. Then, run the following in cmd or PowerShell:

    git clone https://github.com/solvespace/solvespace
    cd solvespace
    git submodule update --init
    mkdir build
    cd build
    cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
    nmake

### Building with MinGW

It is also possible to build SolveSpace using [MinGW][mingw], though
Space Navigator support will be disabled.

First, ensure that git and gcc are in your `$PATH`. Then, run the following
in bash:

    git clone https://github.com/solvespace/solvespace
    cd solvespace
    git submodule update --init
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make

[gitwin]: https://git-scm.com/download/win
[cmakewin]: http://www.cmake.org/download/#latest
[mingw]: http://www.mingw.org/

Contributing
------------

See the [guide for contributors](CONTRIBUTING.md) for the best way to file issues, contribute code,
and debug SolveSpace.

License
-------

SolveSpace is distributed under the terms of the [GPL3 license](COPYING.txt).
