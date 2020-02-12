#!/bin/sh -ex
cd $(dirname $0)
flatpak-builder "$@" --force-clean --repo repo build-dir com.solvespace.SolveSpace.json
flatpak build-bundle repo solvespace.flatpak com.solvespace.SolveSpace
