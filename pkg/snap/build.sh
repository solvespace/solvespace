#!/bin/sh -xe

dir="$(dirname "$(readlink -f "$0")")"
solvespace_snap_src="$dir/solvespace-snap-src"
trap "rm -rf $solvespace_snap_src" EXIT

cd "$dir"

git_root="$(git rev-parse --show-toplevel)"
rsync --filter=":- .gitignore" -r "$git_root"/ "$solvespace_snap_src"

snapcraft "$@"
