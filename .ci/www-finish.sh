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
SSH_REPO="git@github.com:LebedevRI/www.rawspeed.org.git"

# Now let's go have some fun with the cloned repo
cd "$TRAVIS_BUILD_DIR/build/docs/doxygen/html"
git config user.name "Travis CI"
git config user.email "travis@travis-ci.org"

date > .timestamp
touch .nojekyll
echo "rawspeed.org" > CNAME

# # If there are no changes (e.g. this is a README update) then just bail.
# if [[ -z `git diff --exit-code` ]]; then
#     echo "No changes to the spec on this push; exiting."
#     exit 0
# fi

# Commit the "changes", i.e. the new version.
# The delta will show diffs between new and old versions.
git add .
git commit --quiet -m "Deploy to GitHub Pages: ${TRAVIS_COMMIT}"

ENCRYPTION_LABEL="459909475b8a"

# Get the deploy key by using Travis's stored variables to decrypt wro_deploy.enc
ENCRYPTED_KEY_VAR="encrypted_${ENCRYPTION_LABEL}_key"
ENCRYPTED_IV_VAR="encrypted_${ENCRYPTION_LABEL}_iv"
ENCRYPTED_KEY=${!ENCRYPTED_KEY_VAR}
ENCRYPTED_IV=${!ENCRYPTED_IV_VAR}
openssl aes-256-cbc -K $ENCRYPTED_KEY -iv $ENCRYPTED_IV -in "$TRAVIS_BUILD_DIR/.ci/wro_deploy.enc" -out "$TRAVIS_BUILD_DIR/.ci/wro_deploy" -d
chmod 600 "$TRAVIS_BUILD_DIR/.ci/wro_deploy"
eval `ssh-agent -s`
ssh-add "$TRAVIS_BUILD_DIR/.ci/wro_deploy"

# Now that we're all set up, we can push.
git push -f $SSH_REPO $TARGET_BRANCH
