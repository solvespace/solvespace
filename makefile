all: # Default target

.SUFFIXES: # Disables built-in rules
.ONESHELL:

# Parameters
DEB_PACKAGES_COMMON=git \
	build-essential \
	cmake \
	zlib1g-dev \
	libpng-dev \
	libcairo2-dev \
	libfreetype6-dev \
	libjson-c-dev \
	libfontconfig1-dev \
	libgl-dev \
	libglu-dev \
	libspnav-dev

DEB_PACKAGES_GTK3=libgtkmm-3.0-dev \
	libpangomm-1.4-dev

DEB_PACKAGES_GTK4=libgtkmm-4.0-dev

DEB_PACKAGES_QT=qt6-base-dev


# Dependencies
#all: build-gtk3/bin/solvespace
all: build-gtk4/bin/solvespace
#all: build-qt/bin/solvespace
build-gtk3/Makefile: CMAKE_OPT=-DENABLE_GUI=ON
build-gtk4/Makefile: CMAKE_OPT=-DENABLE_GUI=ON -DUSE_GTK4_GUI=ON
build-qt/Makefile: CMAKE_OPT=-DENABLE_GUI=ON -DUSE_QT_GUI=ON


# Recipes
build-%/Makefile:
	mkdir -p $(@D)
	cd $(@D)
	cmake .. $(CMAKE_OPT)

build-%/bin/solvespace: build-%/Makefile
	make -C build-$* -kj


# Commands
.PHONY: tools
tools:
	sudo apt install -y $(DEB_PACKAGES_COMMON) $(DEB_PACKAGES_GTK3) $(DEB_PACKAGES_GTK4) $(DEB_PACKAGES_QT)

.PHONY: clean
clean:
	rm -rf build-*
