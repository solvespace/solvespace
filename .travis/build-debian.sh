#!/bin/sh -xe

if echo $TRAVIS_TAG | grep ^release-; then BUILD_TYPE=RelWithDebInfo; else BUILD_TYPE=Debug; fi

export BUILD_TYPE
dpkg-buildpackage -b -us -uc
