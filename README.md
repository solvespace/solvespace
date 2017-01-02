SolveSpace
==========

This repository contains the source code of [SolveSpace][], a parametric
2d/3d CAD.

[solvespace]: http://solvespace.com

Installation
------------

### macOS (>=10.6 64-bit), Windows (>=XP 32-bit)

Binary packages for macOS and Windows are available via
[GitHub releases][rel].

[rel]: https://github.com/solvespace/solvespace/releases

### Other systems

See below.

Building on Linux
-----------------

### Building for Linux

You will need CMake, zlib, json-c, libpng, cairo, freetype, fontconfig, gtkmm 3.0, pangomm 1.4,
OpenGL and OpenGL GLU, and optionally, the Space Navigator client library.
On a Debian derivative (e.g. Ubuntu) these can be installed with:

    apt-get install cmake libjson-c-dev libpng-dev libcairo2-dev libfreetype6-dev \
                    libfontconfig1-dev libgtkmm-3.0-dev libpangomm-1.4-dev \
                    libgl-dev libglu-dev libspnav-dev

Before building, check out the necessary submodules:

    git submodule update --init extlib/libdxfrw

After that, build SolveSpace as following:

    mkdir build
    cd build
    cmake .. -DENABLE_TESTS=OFF
    make
    sudo make install

The application is built as `build/bin/solvespace`.

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

The application is built as `build/bin/solvespace.exe`.

Space Navigator support will not be available.

Building on macOS
-----------------

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
    cmake .. -DENABLE_TESTS=OFF
    make

The application is built in `build/bin/solvespace.app`, and
the executable file is `build/bin/solvespace.app/Contents/MacOS/solvespace`.

[homebrew]: http://brew.sh/

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

Debugging a crash
-----------------

SolveSpace releases are throughly tested but sometimes they contain crash
bugs anyway. The reason for such crashes can be determined only if the executable
was built with debug information.

### Debugging a released version

The Linux distributions usually include separate debug information packages.
On a Debian derivative (e.g. Ubuntu), these can be installed with:

    apt-get install solvespace-dbg

The macOS releases include the debug information, and no further action
is needed.

The Windows releases include the debug information on the GitHub
[release downloads page](https://github.com/solvespace/solvespace/releases).

### Debugging a custom build

If you are building SolveSpace yourself on a Unix-like platform,
configure or re-configure SolveSpace to produce a debug build, and
then re-build it:

    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Debug [other cmake args...]
    make

If you are building SolveSpace yourself using the Visual Studio IDE,
select Debug from the Solution Configurations list box on the toolbar,
and build the solution.

### Debugging with gdb

gdb is a debugger that is mostly used on Linux. First, run SolveSpace
under debugging:

    gdb [path to solvespace executable]
    (gdb) run

Then, reproduce the crash. After the crash, attach the output in
the console, as well as output of the following gdb commands to
a bug report:

    (gdb) backtrace
    (gdb) info locals

If the crash is not easy to reproduce, please generate a core file,
which you can use to resume the debugging session later, and provide
any other information that is requested:

    (gdb) generate-core-file

This will generate a large file called like `core.1234` in the current
directory; it can be later re-loaded using `gdb --core core.1234`.

### Debugging with lldb

lldb is a debugger that is mostly used on macOS. First, run SolveSpace
under debugging:

    lldb [path to solvespace executable]
    (lldb) run

Then, reproduce the crash. After the crash, attach the output in
the console, as well as output of the following gdb commands to
a bug report:

    (lldb) backtrace all
    (lldb) frame variable

If the crash is not easy to reproduce, please generate a core file,
which you can use to resume the debugging session later, and provide
any other information that is requested:

    (lldb) process save-core "core"

This will generate a large file called `core` in the current
directory; it can be later re-loaded using `lldb -c core`.

License
-------

SolveSpace is distributed under the terms of the [GPL3 license](COPYING.txt).
