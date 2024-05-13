#!/bin/bash -eu
# Copyright 2017 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################

set -ex

apt-get install -y ninja-build
export CMAKE_GENERATOR=Ninja

ln -f -s /usr/local/bin/lld /usr/bin/ld

cd "$SRC"

wget -q https://github.com/llvm/llvm-project/releases/download/llvmorg-16.0.6/llvm-project-16.0.6.src.tar.xz
tar -xf llvm-project-16.0.6.src.tar.xz llvm-project-16.0.6.src/{runtimes,cmake,llvm/cmake,libcxx,libcxxabi}/
LIBCXX_BUILD="$SRC/llvm-project-16.0.6.build"
mkdir "$LIBCXX_BUILD"
cmake -S llvm-project-16.0.6.src/runtimes/ -B "$LIBCXX_BUILD" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=OFF \
      -DLLVM_INCLUDE_TESTS=OFF \
      -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
      -DLIBCXX_ENABLE_SHARED=OFF \
      -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON \
      -DLIBCXXABI_ENABLE_SHARED=OFF \
      -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
      -DLIBCXXABI_ADDITIONAL_COMPILE_FLAGS="-fno-sanitize=vptr"
cmake --build "$LIBCXX_BUILD" -- -j$(nproc) cxx cxxabi

CXXFLAGS="$CXXFLAGS -nostdinc++ -nostdlib++ -isystem $LIBCXX_BUILD/include -isystem $LIBCXX_BUILD/include/c++/v1 -L$LIBCXX_BUILD/lib -lc++ -lc++abi"

if [[ $SANITIZER = *undefined* ]]; then
  CFLAGS="$CFLAGS -fsanitize=unsigned-integer-overflow -fno-sanitize-recover=unsigned-integer-overflow"
  CXXFLAGS="$CXXFLAGS -fsanitize=unsigned-integer-overflow -fno-sanitize-recover=unsigned-integer-overflow"
fi

WITH_OPENMP=ON
if [[ $SANITIZER = *memory* ]]; then
  WITH_OPENMP=OFF
fi

cd "$WORK"
mkdir build
cd build

cmake \
  -DBINARY_PACKAGE_BUILD=ON -DWITH_OPENMP=$WITH_OPENMP \
  -DUSE_BUNDLED_LLVMOPENMP=ON -DALLOW_DOWNLOADING_LLVMOPENMP=ON \
  -DWITH_PUGIXML=OFF -DUSE_XMLLINT=OFF -DWITH_JPEG=OFF -DWITH_ZLIB=OFF \
  -DBUILD_TESTING=OFF -DBUILD_TOOLS=OFF -DBUILD_BENCHMARKING=OFF \
  -DCMAKE_BUILD_TYPE=FUZZ -DBUILD_FUZZERS=ON \
  -DLIB_FUZZING_ENGINE:STRING="$LIB_FUZZING_ENGINE" \
  -DCMAKE_INSTALL_PREFIX:PATH="$OUT" -DCMAKE_INSTALL_BINDIR:PATH="$OUT" \
  "$SRC/librawspeed/"

cmake --build . -- -j$(nproc) all && cmake --build . -- -j$(nproc) install

du -hcs .
du -hcs "$OUT"
