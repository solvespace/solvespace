#!/bin/sh -ex

make -C build solvespace_testsuite
./build/test/solvespace_testsuite $* || true
for e in slvs png; do
  for i in `find . -name *.out.$e`; do
    mv $i `dirname $i`/`basename $i .out.$e`.$e;
  done;
done
