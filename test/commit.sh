#!/bin/sh -ex

make -C build solvespace-testsuite
./build/test/solvespace-testsuite $* || true
for e in slvs png; do
  for i in `find . -name *.out.$e`; do
    mv $i `dirname $i`/`basename $i .out.$e`.$e;
  done;
done
