#!/bin/sh -xe

sudo /snap/bin/lxd waitready
sudo /snap/bin/lxd init --auto
sudo chgrp travis /var/snap/lxd/common/lxd/unix.socket
mkdir -p "$TRAVIS_BUILD_DIR/snaps-cache"
