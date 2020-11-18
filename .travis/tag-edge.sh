#!/bin/sh -xe

if [ -z "$TRAVIS_TAG" ]; then
  git config --local user.name "solvespace-cd"
  git config --local user.email "no-reply@solvespace.com"
  export TRAVIS_TAG="edge"
  git tag --force $TRAVIS_TAG
fi
