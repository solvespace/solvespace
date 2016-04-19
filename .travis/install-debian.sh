#!/bin/sh -xe

sudo apt-get update -qq
sudo apt-get install -q -y \
  cmake cmake-data libpng12-dev zlib1g-dev libjson0-dev libfontconfig1-dev \
  libgtkmm-2.4-dev libpangomm-1.4-dev libgl1-mesa-dev libglu-dev libglew-dev \
  libfreetype6-dev dpkg-dev
