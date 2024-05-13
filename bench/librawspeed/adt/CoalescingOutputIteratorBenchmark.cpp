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

#include "adt/CoalescingOutputIterator.h"
#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "adt/DefaultInitAllocatorAdaptor.h"
#include "bench/Common.h"
#include "common/Common.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>
#include <benchmark/benchmark.h>

namespace rawspeed {

namespace {

template <bool b, typename T> struct C {
  using coalescing = std::bool_constant<b>;
  using value_type = T;
};
using NoCoalescing = C<false, uint8_t>;
template <typename T> using CoalesceTo = C<true, T>;

template <bool ShouldCoalesce, typename UnderlyingOutputIterator>
  requires(!ShouldCoalesce)
auto getMaybeCoalescingOutputIterator(UnderlyingOutputIterator e) {
  return e;
}

template <bool ShouldCoalesce, typename UnderlyingOutputIterator>
  requires(ShouldCoalesce)
auto getMaybeCoalescingOutputIterator(UnderlyingOutputIterator e) {
  return CoalescingOutputIterator(e);
}

template <typename C> void BM_Broadcast(benchmark::State& state) {
  using T = typename C::value_type;

  int64_t numBytes = state.range(0);
  const int bytesPerChunk = sizeof(T);
  const auto numChunks =
      implicit_cast<int>(roundUpDivisionSafe(numBytes, bytesPerChunk));
  numBytes = bytesPerChunk * numChunks;

  std::vector<T, DefaultInitAllocatorAdaptor<T, std::allocator<T>>> output;
  output.reserve(implicit_cast<size_t>(numChunks));

  for (auto _ : state) {
    output.clear();

    auto iter = getMaybeCoalescingOutputIterator<C::coalescing::value>(
        std::back_inserter(output));
    uint8_t value = 0;
    for (int chunkIndex = 0; chunkIndex != numChunks; ++chunkIndex) {
#if defined(__clang__)
#pragma clang loop unroll(full)
#endif
      for (int byteOfChunk = 0; byteOfChunk != bytesPerChunk; ++byteOfChunk) {
        benchmark::DoNotOptimize(value);
        *iter = value;
      }
    }
  }
  assert(implicit_cast<int64_t>(output.size()) == numChunks);

  state.SetComplexityN(numBytes);
  state.counters.insert({
      {"Throughput",
       benchmark::Counter(sizeof(std::byte) * state.complexity_length_n(),
                          benchmark::Counter::Flags::kIsIterationInvariantRate,
                          benchmark::Counter::kIs1024)},
      {"Latency",
       benchmark::Counter(sizeof(std::byte) * state.complexity_length_n(),
                          benchmark::Counter::Flags::kIsIterationInvariantRate |
                              benchmark::Counter::Flags::kInvert,
                          benchmark::Counter::kIs1000)},
  });
}

template <typename C> void BM_Copy(benchmark::State& state) {
  using T = typename C::value_type;

  int64_t numBytes = state.range(0);
  const int bytesPerChunk = sizeof(T);
  const auto numChunks =
      implicit_cast<int>(roundUpDivisionSafe(numBytes, bytesPerChunk));
  numBytes = bytesPerChunk * numChunks;

  std::vector<uint8_t,
              DefaultInitAllocatorAdaptor<uint8_t, std::allocator<uint8_t>>>
      inputStorage(implicit_cast<size_t>(numBytes), uint8_t{0});
  const auto input = Array1DRef(
      inputStorage.data(), rawspeed::implicit_cast<int>(inputStorage.size()));
  benchmark::DoNotOptimize(input.begin());

  std::vector<T, DefaultInitAllocatorAdaptor<T, std::allocator<T>>> output;
  output.reserve(implicit_cast<size_t>(numChunks));

  for (auto _ : state) {
    output.clear();

    auto iter = getMaybeCoalescingOutputIterator<C::coalescing::value>(
        std::back_inserter(output));
    for (int chunkIndex = 0; chunkIndex != numChunks; ++chunkIndex) {
      const auto chunk =
          input.getBlock(bytesPerChunk, chunkIndex).getAsArray1DRef();
#if defined(__clang__)
#pragma clang loop unroll(full)
#endif
      for (int byteOfChunk = 0; byteOfChunk != bytesPerChunk; ++byteOfChunk) {
        *iter = chunk(byteOfChunk);
      }
    }
  }
  assert(implicit_cast<int64_t>(output.size()) == numChunks);

  state.SetComplexityN(numBytes);
  state.counters.insert({
      {"Throughput",
       benchmark::Counter(sizeof(std::byte) * state.complexity_length_n(),
                          benchmark::Counter::Flags::kIsIterationInvariantRate,
                          benchmark::Counter::kIs1024)},
      {"Latency",
       benchmark::Counter(sizeof(std::byte) * state.complexity_length_n(),
                          benchmark::Counter::Flags::kIsIterationInvariantRate |
                              benchmark::Counter::Flags::kInvert,
                          benchmark::Counter::kIs1000)},
  });
}

void CustomArguments(benchmark::internal::Benchmark* b) {
  b->Unit(benchmark::kMicrosecond);

  static constexpr int L1dByteSize = 32U * (1U << 10U);
  static constexpr int L2dByteSize = 512U * (1U << 10U);
  static constexpr int MaxBytesOptimal = L2dByteSize * (1U << 5);

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

// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define GEN(I, C) BENCHMARK(I<C>)->Apply(CustomArguments)

#define GEN_T(I)                                                               \
  GEN(I, NoCoalescing);                                                        \
  GEN(I, CoalesceTo<uint16_t>);                                                \
  GEN(I, CoalesceTo<uint32_t>);                                                \
  GEN(I, CoalesceTo<uint64_t>)

GEN_T(BM_Broadcast);
GEN_T(BM_Copy);

// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace

} // namespace rawspeed

BENCHMARK_MAIN();
