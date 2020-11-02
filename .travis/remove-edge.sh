#!/bin/sh -xe

sudo apt-get update -qq
sudo apt-get -y install jq

old_release=$(curl \
  -H "Accept: application/vnd.github.v3+json" \
  https://api.github.com/repos/solvespace/solvespace/releases/tags/edge \
  | jq -r ".url")

if [ -z "$old_release" ]; then
  curl \
    -X DELETE \
    -H "Accept: application/vnd.github.v3+json" \
    -H "Authorization: token $GITHUB_TOKEN" \
    "$old_release"
fi

curl \
  -X DELETE \
  -H "Accept: application/vnd.github.v3+json" \
  -H "Authorization: token $GITHUB_TOKEN" \
  https://api.github.com/repos/solvespace/solvespace/git/refs/tags/edge
