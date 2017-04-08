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

if [ -z "${MAKEFLAGS+x}" ];
then
  MAKEFLAGS="-j2 -v"
fi

target_build()
{
  # to get as much of the issues into the log as possible
  cmake --build "$BUILD_DIR" -- $MAKEFLAGS || cmake --build "$BUILD_DIR" -- -j1 -v -k0

  ctest --output-on-failure || ctest --rerun-failed -V -VV

  case "$FLAVOR" in
  "Coverage")
    cmake --build "$BUILD_DIR" -- --target gcov
    mkdir "$BUILD_DIR/gcov-reports-unittest"
    find "$BUILD_DIR" -type f -name '*.gcov' -exec mv -t "$BUILD_DIR/gcov-reports-unittest" {} + > /dev/null
    ;;
  *)
    ;;
  esac

  # and now check that it installs where told and only there.
  cmake --build "$BUILD_DIR" --target install -- $MAKEFLAGS || cmake --build "$BUILD_DIR" --target install -- -j1 -v -k0
}

df
du -hcs "$SRC_DIR"
du -hcs "$BUILD_DIR"
du -hcs "$INSTALL_PREFIX"

CMAKE_BUILD_TYPE="RelWithDebInfo"

case "$FLAVOR" in
  "Coverage")
    CMAKE_BUILD_TYPE="Coverage"
    G="Unix Makefiles"
    ;;
  *)
    ;;
esac

GENERATOR="Ninja"
if [ ! -z "${G+x}" ];
then
  GENERATOR="$G"
fi

cd "$BUILD_DIR"
cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" -G"$GENERATOR" -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" $ECO "$SRC_DIR" || (cat "$BUILD_DIR"/CMakeFiles/CMakeOutput.log; cat "$BUILD_DIR"/CMakeFiles/CMakeError.log)

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
