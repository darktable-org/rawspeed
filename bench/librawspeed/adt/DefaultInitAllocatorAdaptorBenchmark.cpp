/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

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

#include "adt/DefaultInitAllocatorAdaptor.h"
#include "adt/Casts.h"
#include "bench/Common.h"
#include <cstddef>
#include <memory>
#include <vector>
#include <benchmark/benchmark.h>

using Type = std::byte;

namespace {

template <typename Allocator> void construct(benchmark::State& state) {
  std::vector<Type, Allocator> vec(
      rawspeed::implicit_cast<size_t>(state.range(0)));
  benchmark::DoNotOptimize(vec);
}

template <typename Allocator>
void construct_with_zeroinit(benchmark::State& state) {
  std::vector<Type, Allocator> vec(
      rawspeed::implicit_cast<size_t>(state.range(0)), Type(0));
  benchmark::DoNotOptimize(vec);
}

template <typename Worker>
void BM_std_vector(benchmark::State& state, Worker worker) {
  // Do it once outside of the loop to maybe offset the initial alloc time.
  worker(state);

  for (auto _ : state)
    worker(state);

  const auto AllocSize = sizeof(Type) * state.range(0);
  state.counters["Allocation,bytes"] = benchmark::Counter(
      rawspeed::implicit_cast<double>(AllocSize), benchmark::Counter::kDefaults,
      benchmark::Counter::kIs1024);
  state.SetComplexityN(AllocSize);
  state.SetBytesProcessed(AllocSize * state.iterations());
  state.SetItemsProcessed(state.range(0) * state.iterations());
}

void CustomArguments(benchmark::internal::Benchmark* b) {
  if (benchmarkDryRun()) {
    b->Arg(512U * (1U << 10U)); // 512 KiB
    return;
  }

  b->RangeMultiplier(2)
      ->Range(1U << 0U, 1U << 31U)
      ->Complexity(benchmark::BigO::oN);
}

#define BENCHMARK_CAPTURE_NAME(func, ...)                                      \
  BENCHMARK_CAPTURE(func, #__VA_ARGS__, __VA_ARGS__)

BENCHMARK_CAPTURE_NAME(BM_std_vector, construct<std::allocator<Type>>)
    ->Apply(CustomArguments);
BENCHMARK_CAPTURE_NAME(
    BM_std_vector,
    construct<
        rawspeed::DefaultInitAllocatorAdaptor<Type, std::allocator<Type>>>)
    ->Apply(CustomArguments);

BENCHMARK_CAPTURE_NAME(BM_std_vector,
                       construct_with_zeroinit<std::allocator<Type>>)
    ->Apply(CustomArguments);
BENCHMARK_CAPTURE_NAME(
    BM_std_vector,
    construct_with_zeroinit<
        rawspeed::DefaultInitAllocatorAdaptor<Type, std::allocator<Type>>>)
    ->Apply(CustomArguments);

} // namespace

BENCHMARK_MAIN();
