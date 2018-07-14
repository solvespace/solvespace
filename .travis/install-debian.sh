#!/bin/sh -xe

sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get update -qq
sudo apt-get install -q -y \
  cmake cmake-data libc6-dev libpng12-dev zlib1g-dev libjson0-dev libfontconfig1-dev \
  libgtkmm-3.0-dev libpangomm-1.4-dev libcairo2-dev libgl1-mesa-dev libglu-dev \
  libfreetype6-dev dpkg-dev gcc-5 g++-5 lcov
