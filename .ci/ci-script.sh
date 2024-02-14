#!/bin/sh

# it is supposed to be run by ci
# expects a few env variables to be set:
#   BUILD_DIR - the working directory, where to build
#   INSTALL_DIR - the installation prefix.
#   SRC_DIR - read-only directory with git checkout to compile
#   CC, CXX, CFLAGS, CXXFLAGS are not required, should make sense too
#   TARGET - either build or usermanual
#   ECO - some other flags for cmake

set -ex

CMAKE_BUILD_TYPE="ReleaseWithAsserts"
GENERATOR="Ninja"
VERBOSE="-v"
KEEPGOING="-k0"

case "$FLAVOR" in
  "Release")
    CMAKE_BUILD_TYPE="Release"
    ;;
  "ReleaseWithAsserts" | "ClangTidy" | "ClangStaticAnalysis" | "ClangCTUStaticAnalysis" | "CodeQLAnalysis" | "SonarCloudStaticAnalysis")
    CMAKE_BUILD_TYPE="ReleaseWithAsserts"
    ;;
  "Coverage")
    CMAKE_BUILD_TYPE="Coverage"
    ;;
  *)
    exit 1
    ;;
esac

case "$TARGET" in
  "WWW")
    ECO="${ECO} -DBUILD_DOCS=ON"
    ;;
  *)
    ;;
esac

if [ ! -z "${G+x}" ];
then
  GENERATOR="$G"
fi

if [ -z "${MAKEFLAGS+x}" ];
then
  MAKEFLAGS="$VERBOSE $KEEPGOING"
fi

target_configure()
{
  cmake -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" -G"$GENERATOR" -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" $ECO "$SRC_DIR" || (cat "$BUILD_DIR"/CMakeFiles/CMakeOutput.log; cat "$BUILD_DIR"/CMakeFiles/CMakeError.log)
}

target_build()
{
  # to get as much of the issues into the log as possible
  cmake --build "$BUILD_DIR" -- $MAKEFLAGS || cmake --build "$BUILD_DIR" -- -j1 $MAKEFLAGS
}

target_test()
{
  # We don't really want to always provide gcov-clean CMake target,
  # but also don't really want to complicate this script,
  # so just consume the error if the target is not there.
  cmake --build "$BUILD_DIR" --target gcov-clean || true
  ctest --label-exclude benchmark --output-on-failure || ctest --label-exclude benchmark --output-on-failure --rerun-failed -V -VV
}

target_test_benchmarks()
{
  # We don't really want to always provide gcov-clean CMake target,
  # but also don't really want to complicate this script,
  # so just consume the error if the target is not there.
  cmake --build "$BUILD_DIR" --target gcov-clean || true
  ctest --label-regex benchmark --output-on-failure || ctest --label-regex benchmark --rerun-failed -V -VV
}

target_test_integration()
{
  if [ "$FLAVOR" = "Coverage" ]; then
    cmake --build "$BUILD_DIR" --target gcov-clean
  fi
  cmake --build "$BUILD_DIR" --target rstest-check
}

handle_coverage_data()
{
  cmake --build "$BUILD_DIR" --target gcov
  mkdir "$BUILD_DIR/gcov-reports-unittest"
  # Can't use \+ because OSX's mv does not have --target-directory, and \+ must
  # come right after {} (the target directory can not be specified inbetween)
  find "$BUILD_DIR" -maxdepth 1 -iname '*.gcov' -exec mv "{}" "$BUILD_DIR/gcov-reports-unittest" \;
}

target_coverage_benchmarks_data()
{
  cmake --build "$BUILD_DIR" --target gcov
  mkdir "$BUILD_DIR/gcov-reports-benchmarks"
  # Can't use \+ because OSX's mv does not have --target-directory, and \+ must
  # come right after {} (the target directory can not be specified inbetween)
  find "$BUILD_DIR" -maxdepth 1 -iname '*.gcov' -exec mv "{}" "$BUILD_DIR/gcov-reports-benchmarks" \;
}

target_coverage_integration_data()
{
  cmake --build "$BUILD_DIR" --target gcov
  mkdir "$BUILD_DIR/gcov-reports-rsa"
  # Can't use \+ because OSX's mv does not have --target-directory, and \+ must
  # come right after {} (the target directory can not be specified inbetween)
  find "$BUILD_DIR" -maxdepth 1 -iname '*.gcov' -exec mv "{}" "$BUILD_DIR/gcov-reports-rsa" \;
}

target_install()
{
  # and now check that it installs where told and only there.
  cmake --build "$BUILD_DIR" --target install -- $MAKEFLAGS || cmake --build "$BUILD_DIR" --target install -- -j1 $MAKEFLAGS
}

target_www()
{
  cmake --build "$BUILD_DIR" -- $VERBOSE docs
}

diskspace()
{
  df
  du -hcs "$SRC_DIR"
  du -hcs "$BUILD_DIR"
  du -hcs "$INSTALL_PREFIX"
}

diskspace

cd "$BUILD_DIR"

case "$TARGET" in
  "configure")
  target_configure
    ;;
  "build")
    target_build
    ;;
  "test")
    target_test
    ;;
  "test_benchmarks")
    target_test_benchmarks
    ;;
  "test_integration")
    target_test_integration
    ;;
  "coverage")
    handle_coverage_data
    ;;
  "coverage_benchmarks")
    target_coverage_benchmarks_data
    ;;
  "coverage_integration")
    target_coverage_integration_data
    ;;
  "install")
    target_install
    ;;
  "WWW")
    target_www
    ;;
  *)
    exit 1
    ;;
esac

diskspace
