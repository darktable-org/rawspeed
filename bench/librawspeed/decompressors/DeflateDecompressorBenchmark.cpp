/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "decompressors/DeflateDecompressor.h"
#include "adt/Casts.h"
#include "adt/Point.h"
#include "bench/Common.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "io/Buffer.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>
#include <zconf.h>
#include <zlib.h>
#include <benchmark/benchmark.h>

#ifndef NDEBUG
#include <limits>
#endif

using rawspeed::Buffer;
using rawspeed::DeflateDecompressor;

namespace {

template <size_t N> using BPS = std::integral_constant<size_t, N>;
template <int N> using Pf = std::integral_constant<int, N>;

template <typename BPS>
std::vector<uint8_t> compressChunk(const rawspeed::RawImage& mRaw,
                                   uLong* bufSize) {
  static_assert(BPS::value > 0, "bad bps");
  static_assert(rawspeed::isAligned(BPS::value, 8), "not byte count");

  const uLong uncompressedLength = BPS::value * mRaw->dim.x * mRaw->dim.y / 8UL;
  assert(uncompressedLength > 0);
  assert(uncompressedLength <= std::numeric_limits<Buffer::size_type>::max());

  *bufSize = compressBound(uncompressedLength);
  assert(*bufSize > 0);
  assert(*bufSize <= std::numeric_limits<Buffer::size_type>::max());

  const std::vector<uint8_t> uBuf(uncompressedLength);
  std::vector<uint8_t> cBuf(*bufSize);

  const int err =
      compress(cBuf.data(), bufSize, uBuf.data(), uncompressedLength);
  if (err != Z_OK)
    throw;

  assert(compressBound(uncompressedLength) >= *bufSize);

  return cBuf;
}

template <typename BPS, typename Pf>
inline void BM_DeflateDecompressor(benchmark::State& state) {
  static_assert(BPS::value > 0, "bad bps");
  static_assert(rawspeed::isAligned(BPS::value, 8), "not byte count");

  const auto dim = areaToRectangle(state.range(0));
  auto mRaw = rawspeed::RawImage::create(dim, rawspeed::RawImageType::F32, 1);

  uLong cBufSize;
  const std::vector<uint8_t> cBuf = compressChunk<BPS>(mRaw, &cBufSize);
  assert(cBufSize > 0);

  Buffer buf(cBuf.data(), rawspeed::implicit_cast<Buffer::size_type>(cBufSize));
  assert(buf.getSize() == cBufSize);

  int predictor = 0;
  switch (Pf::value) {
  case 1:
    predictor = 3;
    break;
  case 2:
    predictor = 34894;
    break;
  case 4:
    predictor = 34895;
    break;
  default:
    __builtin_unreachable();
    break;
  }

  // NOLINTNEXTLINE(modernize-avoid-c-arrays)
  std::unique_ptr<unsigned char[]> uBuffer;

  for (auto _ : state) {
    DeflateDecompressor d(buf, mRaw, predictor, BPS::value);

    d.decode(&uBuffer, mRaw->dim, mRaw->dim, {0, 0});
  }

  state.SetComplexityN(dim.area());
  state.SetItemsProcessed(state.complexity_length_n() * state.iterations());
  state.SetBytesProcessed(BPS::value * state.items_processed() / 8);
}

inline void CustomArgs(benchmark::internal::Benchmark* b) {
  if (benchmarkDryRun()) {
    static constexpr int L2dByteSize = 512U * (1U << 10U);
    b->Arg((L2dByteSize / (32 / 8)) / 4);
    return;
  }

  b->RangeMultiplier(2);
  // FIXME: appears to not like 1GPix+ buffers
  if constexpr ((true)) {
    b->Arg(128 << 20);
  } else {
    b->Range(1, 1023 << 20)->Complexity(benchmark::oN);
  }
  b->Unit(benchmark::kMillisecond);
}

#define GEN_E(s, f)                                                            \
  BENCHMARK_TEMPLATE(BM_DeflateDecompressor, BPS<s>, Pf<f>)->Apply(CustomArgs);
#define GEN_PFS(s) GEN_E(s, 1) GEN_E(s, 2) GEN_E(s, 4)
#define GEN_PSS() GEN_PFS(16) GEN_PFS(24) GEN_PFS(32)

GEN_PSS()

} // namespace

BENCHMARK_MAIN();
