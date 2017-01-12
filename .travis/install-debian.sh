#!/bin/sh -xe

wget -O - http://apt.llvm.org/llvm-snapshot.gpg.key|sudo apt-key add -
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo add-apt-repository -y 'deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-3.9 main'
sudo apt-get update -qq
sudo apt-get install -q -y \
  cmake cmake-data libpng12-dev zlib1g-dev libjson0-dev libfontconfig1-dev \
  libgtkmm-3.0-dev libpangomm-1.4-dev libcairo2-dev libgl1-mesa-dev libglu-dev \
  libfreetype6-dev dpkg-dev libstdc++-5-dev clang-3.9 clang++-3.9 lcov
