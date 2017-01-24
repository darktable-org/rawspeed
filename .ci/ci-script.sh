#!/bin/sh

# it is supposed to be run by travis-ci
# expects a few env variables to be set:
#   BUILD_DIR - the working directory, where to build
#   INSTALL_DIR - the installation prefix.
#   SRC_DIR - read-only directory with git checkout to compile
#   CC, CXX, CFLAGS, CXXFLAGS are not required, should make sense too
#   TARGET - either build or usermanual
#   ECO - some other flags for cmake

set -ex

PARALLEL="-j2"

target_build()
{
  # to get as much of the issues into the log as possible
  cmake --build "$BUILD_DIR" -- $PARALLEL -v || cmake --build "$BUILD_DIR" -- -j1 -v -k0

  ctest --output-on-failure || ctest --rerun-failed -V -VV

  # and now check that it installs where told and only there.
  cmake --build "$BUILD_DIR" --target install -- $PARALLEL -v || cmake --build "$BUILD_DIR" --target install -- -j1 -v -k0
}

du -hcs "$SRC_DIR"
du -hcs "$BUILD_DIR"
du -hcs "$INSTALL_PREFIX"

CMAKE_BUILD_TYPE="RelWithDebInfo"

case "$FLAVOR" in
  "Coverage")
    CMAKE_BUILD_TYPE="Coverage"
    ;;
  *)
    ;;
esac

cd "$BUILD_DIR"
cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" -GNinja -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" $ECO -DUSE_CLANG_TIDY=ON "$SRC_DIR" || (cat "$BUILD_DIR"/CMakeFiles/CMakeOutput.log; cat "$BUILD_DIR"/CMakeFiles/CMakeError.log)

case "$TARGET" in
  "build")
    target_build
    ;;
  *)
    exit 1
    ;;
esac

du -hcs "$SRC_DIR"
du -hcs "$BUILD_DIR"
du -hcs "$INSTALL_PREFIX"
