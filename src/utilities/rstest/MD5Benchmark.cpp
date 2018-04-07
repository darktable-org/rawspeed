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

#include "md5.h"                     // for md5_hash, md5_state
#include <benchmark/benchmark.h>     // for State, Benchmark, BENCHMARK
#include <cstdint>                   // for uint8_t
#include <cstdlib>                   // for free, malloc, size_t
#include <memory>                    // for unique_ptr

static inline void BM_MD5(benchmark::State& state) {
  const size_t bufsize = state.range(0) * sizeof(char);
  std::unique_ptr<char, decltype(&free)> buf((char*)malloc(bufsize), &free);

  for (auto _ : state) {
    rawspeed::md5::md5_state hash;
    rawspeed::md5::md5_hash((uint8_t*)buf.get(), bufsize, &hash);
  }

  state.SetComplexityN(state.range(0));
  state.SetItemsProcessed(state.complexity_length_n() * state.iterations());
  state.SetBytesProcessed(1UL * sizeof(char) * state.items_processed());
}

static inline void CustomArguments(benchmark::internal::Benchmark* b) {
  b->RangeMultiplier(2);
#if 1
  b->Arg(256 << 20);
#else
  b->Range(1, 1024 << 20)->Complexity(benchmark::oN);
#endif
  b->Unit(benchmark::kMillisecond);
}

BENCHMARK(BM_MD5)->Apply(CustomArguments);

BENCHMARK_MAIN()
