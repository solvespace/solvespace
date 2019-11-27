#!/bin/sh -xe

brew update
brew install freetype cairo
git submodule update --init
