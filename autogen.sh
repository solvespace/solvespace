#!/bin/sh

set -ex

autoreconf --force --install

rm -f config.h.in~
rm -rf autom4te.cache

# EOF
