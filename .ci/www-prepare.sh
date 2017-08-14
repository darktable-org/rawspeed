#!/bin/bash

set -e # Exit with nonzero exit code if anything fails

SOURCE_BRANCH="develop"
TARGET_BRANCH="gh-pages"

# Pull requests and commits to other branches shouldn't try to deploy
if [ "$TRAVIS_PULL_REQUEST" != "false" -o "$TRAVIS_BRANCH" != "$SOURCE_BRANCH" ]; then
  exit 1
fi

# Save some useful information
REPO="https://github.com/LebedevRI/www.rawspeed.org.git"

mkdir -p "$TRAVIS_BUILD_DIR/build/docs/"
cd "$TRAVIS_BUILD_DIR/build/docs/"

# Clone the existing gh-pages for this repo into out/
# Create a new empty branch if gh-pages doesn't exist yet (should only happen on first deply)
git clone --depth 1 $REPO html
cd html
git checkout $TARGET_BRANCH || git checkout --orphan $TARGET_BRANCH
cd ..

# Clean out existing contents
rm -rf html/**/* || exit 0
