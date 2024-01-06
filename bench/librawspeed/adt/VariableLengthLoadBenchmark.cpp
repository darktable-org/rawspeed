/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2024 Roman Lebedev

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

#include "adt/VariableLengthLoad.h"
#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "bench/Common.h"
#include "common/Common.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <benchmark/benchmark.h>

namespace rawspeed {

namespace {}

} // namespace rawspeed

namespace {

using rawspeed::variableLengthLoadNaiveViaMemcpy;

template <decltype(variableLengthLoadNaiveViaMemcpy) Impl, typename T>
void BM_Impl(benchmark::State& state) {
  constexpr int bytesPerItem = sizeof(T);

  int64_t numBytes = rawspeed::roundUp(state.range(0), bytesPerItem);
  benchmark::DoNotOptimize(numBytes);

  const std::vector<uint8_t> inStorage(
      rawspeed::implicit_cast<size_t>(numBytes));

  const auto in = rawspeed::Array1DRef<const uint8_t>(
      inStorage.data(), rawspeed::implicit_cast<int>(numBytes));

  std::array<uint8_t, bytesPerItem> outStorage;
  auto out = rawspeed::Array1DRef<uint8_t>(
      outStorage.data(), rawspeed::implicit_cast<int>(bytesPerItem));

  for (auto _ : state) {
    for (int inPos = 0; inPos < numBytes; inPos += bytesPerItem) {
      Impl(out, in, inPos);
      benchmark::DoNotOptimize(out.begin());
    }
  }

  state.SetComplexityN(numBytes);
  state.counters.insert({
      {"Throughput",
       benchmark::Counter(sizeof(uint8_t) * state.complexity_length_n(),
                          benchmark::Counter::Flags::kIsIterationInvariantRate,
                          benchmark::Counter::kIs1024)},
      {"Latency",
       benchmark::Counter(sizeof(uint8_t) * state.complexity_length_n(),
                          benchmark::Counter::Flags::kIsIterationInvariantRate |
                              benchmark::Counter::Flags::kInvert,
                          benchmark::Counter::kIs1000)},
  });
}

void CustomArguments(benchmark::internal::Benchmark* b) {
  b->Unit(benchmark::kMicrosecond);

  static constexpr int L1dByteSize = 32U * (1U << 10U);
  static constexpr int L2dByteSize = 512U * (1U << 10U);
  static constexpr int MaxBytesOptimal = L2dByteSize;

  if (benchmarkDryRun()) {
    b->Arg(L1dByteSize);
    return;
  }

  b->RangeMultiplier(2);
  if constexpr ((true))
    b->Arg(MaxBytesOptimal);
  else
    b->Range(1, 2048UL << 20)->Complexity(benchmark::oN);
}

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#define GEN(I, T) BENCHMARK(BM_Impl<I, T>)->Apply(CustomArguments)

#define GEN_CALLABLE(I)                                                        \
  GEN(I, uint8_t);                                                             \
  GEN(I, uint16_t);                                                            \
  GEN(I, uint32_t);                                                            \
  GEN(I, uint64_t)

#define GEN_TIME() GEN_CALLABLE(GEN_WRAPPER(variableLengthLoadNaiveViaMemcpy))

#undef GEN_WRAPPER
#define GEN_WRAPPER(I) I

GEN_TIME();

// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace

BENCHMARK_MAIN();
