SolveSpace
==========

This repository contains the official repository of [SolveSpace][].

[solvespace]: http://solvespace.com

Installation
------------

All binary packages are built from the `compat` branch.

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

You will need cmake, libpng, zlib, json-c, fontconfig, gtkmm 2.4, pangomm 1.4,
OpenGL and OpenGL GLU.
On a Debian derivative (e.g. Ubuntu) these packages can be installed with:

    apt-get install libpng12-dev libjson-c-dev libfontconfig1-dev \
                    libgtkmm-2.4-dev libpangomm-1.4-dev libgl-dev libglu-dev \
                    libglew-dev

After that, build SolveSpace as following:

    mkdir cbuild
    cd cbuild
    cmake ..
    make
    sudo make install

A fully functional port to GTK3 is available, but not recommended
for use due to bugs in this toolkit.

Building on Mac OS X
--------------------

You will need XCode tools, cmake and libpng. Assuming you use [homebrew][],
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

License
-------

SolveSpace is distributed under the terms of the [GPL3 license](COPYING.txt).
