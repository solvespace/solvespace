#!/bin/sh -xe

sudo apt-get update
sudo ./pkg/snap/build.sh --destructive-mode
