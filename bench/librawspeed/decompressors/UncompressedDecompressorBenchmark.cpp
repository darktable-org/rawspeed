/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2023 Roman Lebedev

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

#include "decompressors/UncompressedDecompressor.h"
#include "adt/Casts.h"
#include "adt/Point.h"
#include "bench/Common.h"
#include "bitstreams/BitStreams.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <type_traits>
#include <vector>
#include <benchmark/benchmark.h>

using rawspeed::BitOrder;
using rawspeed::Buffer;
using rawspeed::UncompressedDecompressor;

namespace {

template <size_t N> using BPS = std::integral_constant<size_t, N>;

template <typename T, BitOrder BO, typename BPS>
inline void BM_UncompressedDecompressor(benchmark::State& state) {
  static_assert(BPS::value > 0, "bad bps");

  auto dim = areaToRectangle(state.range(0));

  int inputPitchBits = BPS::value * dim.x;
  inputPitchBits = rawspeed::implicit_cast<int>(
      rawspeed::roundUp(inputPitchBits, std::lcm(8, BPS::value)));
  assert(inputPitchBits % 8 == 0);
  int inputPitchBytes = inputPitchBits / 8;
  assert(inputPitchBits % BPS::value == 0);
  dim.x = inputPitchBits / BPS::value;

  auto packedLength = rawspeed::implicit_cast<int>(
      rawspeed::roundUp(inputPitchBytes * dim.y, 4));
  const std::vector<uint8_t> buf(packedLength);
  const rawspeed::ByteStream bs(rawspeed::DataBuffer(
      Buffer(buf.data(), packedLength), rawspeed::Endianness::little));

  auto mRaw = rawspeed::RawImage::create(dim,
                                         std::is_same_v<T, float>
                                             ? rawspeed::RawImageType::F32
                                             : rawspeed::RawImageType::UINT16,
                                         1);

  for (auto _ : state) {
    UncompressedDecompressor d(bs, mRaw,
                               rawspeed::iRectangle2D({0, 0}, mRaw->dim),
                               inputPitchBytes, BPS::value, BO);
    d.readUncompressedRaw();
  }

  state.SetComplexityN(dim.area());
  state.SetItemsProcessed(state.complexity_length_n() * state.iterations());
  state.SetBytesProcessed(BPS::value * state.items_processed() / 8);
}

inline void CustomArgs(benchmark::internal::Benchmark* b) {
  b->Unit(benchmark::kMicrosecond);

  if (benchmarkDryRun()) {
    static constexpr int L2dByteSize = 512U * (1U << 10U);
    b->Arg((L2dByteSize / (32 / 8)) / 2);
    return;
  }

  b->RangeMultiplier(2);
  b->Range(1, 1 * 1024 * 1024)->Complexity(benchmark::oN);
}

#define GEN_INNER(a, b, c)                                                     \
  BENCHMARK_TEMPLATE(BM_UncompressedDecompressor, a, b, BPS<c>)                \
      ->Apply(CustomArgs);

#define GEN_F_BPS(b, c) GEN_INNER(float, b, c)

#define GEN_F(b)                                                               \
  GEN_F_BPS(b, 16)                                                             \
  GEN_F_BPS(b, 24)                                                             \
  GEN_F_BPS(b, 32)

#define GEN_U_BPS(b, c) GEN_INNER(uint16_t, b, c)

#define GEN_U(b)                                                               \
  GEN_U_BPS(b, 1)                                                              \
  GEN_U_BPS(b, 2)                                                              \
  GEN_U_BPS(b, 3)                                                              \
  GEN_U_BPS(b, 4)                                                              \
  GEN_U_BPS(b, 5)                                                              \
  GEN_U_BPS(b, 6)                                                              \
  GEN_U_BPS(b, 7)                                                              \
  GEN_U_BPS(b, 8)                                                              \
  GEN_U_BPS(b, 9)                                                              \
  GEN_U_BPS(b, 10)                                                             \
  GEN_U_BPS(b, 11)                                                             \
  GEN_U_BPS(b, 12)                                                             \
  GEN_U_BPS(b, 13)                                                             \
  GEN_U_BPS(b, 14)                                                             \
  GEN_U_BPS(b, 15)                                                             \
  GEN_U_BPS(b, 16)

#define GEN_BO(b) GEN_U(b) GEN_F(b)

GEN_BO(BitOrder::LSB)
GEN_BO(BitOrder::MSB)
GEN_U(BitOrder::MSB16)
GEN_U(BitOrder::MSB32)

} // namespace

BENCHMARK_MAIN();
