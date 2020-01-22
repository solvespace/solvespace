#!/bin/sh -e

channels="$1"
echo "$SNAP_TOKEN" | snapcraft login --with -

for snap in ./pkg/snap/*.snap; do
  snapcraft push "$snap" --release "$channels"
done
