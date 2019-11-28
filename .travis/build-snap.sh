#!/bin/sh -xe

lp_data_dir="$HOME/.local/share/snapcraft/provider/launchpad"
lp_credentials="$lp_data_dir/credentials"

mkdir -p "$lp_data_dir"
openssl aes-256-cbc -K $encrypted_02880a344e9c_key -iv $encrypted_02880a344e9c_iv \
  -in .travis/launchpad-credentials.enc \
  -out "$lp_credentials" -d
chmod 600 "$lp_credentials" 

./pkg/snap/build.sh remote-build \
  --launchpad-user ppd \
  --launchpad-accept-public-upload \
  --build-on=amd64,arm64,armhf,i386
