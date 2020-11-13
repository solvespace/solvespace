#!/bin/sh -xe

# a meaningless change

sudo apt-get update
sudo ./pkg/snap/build.sh --destructive-mode
