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

#include "bitstreams/BitStreamerJPEG.h"
#include "adt/Array1DRef.h"
#include "adt/Invariant.h"
#include "adt/Optional.h"
#include "bench/Common.h"
#include "bitstreams/BitStreamJPEGUtils.h"
#include "bitstreams/BitStreamerMSB.h"
#include <cassert>
#include <cstdint>
#include <benchmark/benchmark.h>

#ifndef NDEBUG
#include <limits>
#endif

namespace rawspeed {

namespace {

template <typename T> void BM(benchmark::State& state, bool Stuffed) {
  int64_t numBytes = state.range(0);
  assert(numBytes > 0);
  assert(numBytes <= std::numeric_limits<int>::max());

  Optional<JPEGStuffedByteStreamGenerator> genStuffed;
  Optional<NonJPEGByteStreamGenerator> genUnstuffed;
  Optional<Array1DRef<const uint8_t>> input;
  if (Stuffed) {
    genStuffed.emplace(numBytes, /*AppendStuffingByte=*/true);
    numBytes = genStuffed->numBytesGenerated;
    input = genStuffed->getInput();
  } else {
    genUnstuffed.emplace(numBytes);
    numBytes = genUnstuffed->numBytesGenerated;
    input = genUnstuffed->getInput();
  }
  benchmark::DoNotOptimize(input->begin());

  for (auto _ : state) {
    T bs(*input);

    constexpr int MaxGetBits = 32;
    int processedBytes = 0;
    for (processedBytes = 0; processedBytes != numBytes;
         processedBytes += MaxGetBits / 8) {
      uint32_t bits = bs.getBits(MaxGetBits);
      benchmark::DoNotOptimize(bits);
    }
    invariant(numBytes == processedBytes);
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
  b->RangeMultiplier(2);

  static constexpr int L1dByteSize = 32U * (1U << 10U);
  static constexpr int L2dByteSize = 512U * (1U << 10U);
  static constexpr int MaxBytesOptimal = L2dByteSize * (1U << 5);

  if (benchmarkDryRun()) {
    b->Arg(L1dByteSize);
    return;
  }

  if constexpr ((true)) {
    b->Arg(MaxBytesOptimal);
  } else {
    b->Range(8, MaxBytesOptimal * (1U << 2));
    b->Complexity(benchmark::oN);
  }
}

#ifndef BENCHMARK_TEMPLATE1_CAPTURE
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define BENCHMARK_TEMPLATE1_CAPTURE(func, a, test_case_name, ...)              \
  BENCHMARK_PRIVATE_DECLARE(func) =                                            \
      (::benchmark::internal::RegisterBenchmarkInternal(                       \
          new ::benchmark::internal::FunctionBenchmark(                        \
              #func "<" #a ">"                                                 \
                    "/" #test_case_name,                                       \
              [](::benchmark::State& st) { func<a>(st, __VA_ARGS__); })))
#endif // BENCHMARK_TEMPLATE1_CAPTURE

BENCHMARK_TEMPLATE1_CAPTURE(BM, BitStreamerJPEG, Stuffed, true)
    ->Apply(CustomArguments);
BENCHMARK_TEMPLATE1_CAPTURE(BM, BitStreamerJPEG, Unstuffed, false)
    ->Apply(CustomArguments);
BENCHMARK_TEMPLATE1_CAPTURE(BM, BitStreamerMSB, Unstuffed, false)
    ->Apply(CustomArguments);

} // namespace

} // namespace rawspeed

BENCHMARK_MAIN();
