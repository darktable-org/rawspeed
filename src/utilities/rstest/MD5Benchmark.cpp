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

#include "md5.h"
#include "adt/Casts.h"
#include "bench/Common.h"
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <benchmark/benchmark.h>

namespace {

void BM_MD5(benchmark::State& state) {
  // Create a zero-initialized data. Content does not matter for our purpose.
  std::vector<uint8_t> buf(rawspeed::implicit_cast<size_t>(state.range(0)),
                           uint8_t(0));

  for (auto _ : state) {
    auto hash = rawspeed::md5::md5_hash(buf.data(), buf.size());
    benchmark::DoNotOptimize(hash);
  }

  state.SetComplexityN(buf.size());
  state.counters.insert({
      {"Throughput",
       benchmark::Counter(sizeof(uint8_t) * state.complexity_length_n(),
                          benchmark::Counter::Flags::kIsIterationInvariantRate,
                          benchmark::Counter::kIs1024)},
      {"Latency",
       benchmark::Counter(sizeof(uint8_t) * state.complexity_length_n(),
                          benchmark::Counter::Flags::kIsIterationInvariantRate |
                              benchmark::Counter::Flags::kInvert,
                          benchmark::Counter::kIs1024)},
  });
}

void CustomArguments(benchmark::internal::Benchmark* b) {
  b->Unit(benchmark::kMillisecond);

  static constexpr int L2dByteSize = 512U * (1U << 10U);
  static constexpr int MaxBytesOptimal = 25 * 1000 * 1000 * sizeof(uint16_t);

  if (benchmarkDryRun()) {
    b->Arg(L2dByteSize);
    return;
  }

  b->RangeMultiplier(2);
  if constexpr ((true))
    b->Arg(MaxBytesOptimal);
  else
    b->Range(1, 2048UL << 20)->Complexity(benchmark::oN);
}

} // namespace

BENCHMARK(BM_MD5)->Apply(CustomArguments);

BENCHMARK_MAIN();
