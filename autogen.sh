#!/bin/sh

set -ex

autoreconf --force --install --warnings=all

rm -f config.h.in~
rm -rf autom4te.cache

# EOF
