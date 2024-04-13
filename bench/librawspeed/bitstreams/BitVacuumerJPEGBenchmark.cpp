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

#include "bitstreams/BitVacuumerJPEG.h"
#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "adt/CoalescingOutputIterator.h"
#include "adt/DefaultInitAllocatorAdaptor.h"
#include "adt/Optional.h"
#include "adt/PartitioningOutputIterator.h"
#include "bench/Common.h"
#include "bitstreams/BitStreamJPEGUtils.h"
#include "bitstreams/BitVacuumerMSB.h"
#include "common/Common.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <type_traits>
#include <vector>
#include <benchmark/benchmark.h>

#ifndef NDEBUG
#include <limits>
#endif

namespace rawspeed {

namespace {

struct BitstreamFlavorMSB;
struct BitstreamFlavorJPEG;

template <typename T> struct BitStreamRoundtripTypes final {};

template <> struct BitStreamRoundtripTypes<BitstreamFlavorMSB> final {
  template <typename OutputIterator>
  using vacuumer = BitVacuumerMSB<OutputIterator>;
};

template <> struct BitStreamRoundtripTypes<BitstreamFlavorJPEG> final {
  template <typename OutputIterator>
  using vacuumer = BitVacuumerJPEG<OutputIterator>;
};

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

template <typename T, typename C>
void BM(benchmark::State& state, bool Stuffed) {
  int64_t numBytes = state.range(0);
  assert(numBytes > 0);
  assert(numBytes <= std::numeric_limits<int>::max());

  Optional<JPEGStuffedByteStreamGenerator> genStuffed;
  Optional<NonJPEGByteStreamGenerator> genUnstuffed;
  Optional<Array1DRef<const uint8_t>> input;
  if (Stuffed) {
    genStuffed.emplace(numBytes, /*AppendStuffingByte=*/false);
    numBytes = genStuffed->numBytesGenerated;
    input = genStuffed->getInput();
  } else {
    genUnstuffed.emplace(numBytes);
    numBytes = genUnstuffed->numBytesGenerated;
    input = genUnstuffed->getInput();
  }
  benchmark::DoNotOptimize(input->begin());

  using OutputChunkType = typename C::value_type;
  std::vector<OutputChunkType,
              DefaultInitAllocatorAdaptor<OutputChunkType,
                                          std::allocator<OutputChunkType>>>
      output;
  output.reserve(implicit_cast<size_t>(
      roundUpDivisionSafe(input->size(), sizeof(OutputChunkType))));

  for (auto _ : state) {
    output.clear();

    auto bsInserter = PartitioningOutputIterator(
        getMaybeCoalescingOutputIterator<C::coalescing::value>(
            std::back_inserter(output)));
    using BitVacuumer = typename BitStreamRoundtripTypes<T>::template vacuumer<
        decltype(bsInserter)>;
    auto bv = BitVacuumer(bsInserter);

    int count = 8;
    for (auto bits : *input) {
      benchmark::DoNotOptimize(bits);
      benchmark::DoNotOptimize(count);
      bv.put(bits, count);
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
  b->RangeMultiplier(2);

  static constexpr int L1dByteSize = 32U * (1U << 10U);
  static constexpr int L2dByteSize = 512U * (1U << 10U);
  static constexpr int MaxBytesOptimal = L2dByteSize * (1U << 2);

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

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#ifndef BENCHMARK_TEMPLATE2_CAPTURE
#define BENCHMARK_TEMPLATE2_CAPTURE(func, a, b, test_case_name, ...)           \
  BENCHMARK_PRIVATE_DECLARE(func) =                                            \
      (::benchmark::internal::RegisterBenchmarkInternal(                       \
          new ::benchmark::internal::FunctionBenchmark(                        \
              #func "<" #a "," #b ">"                                          \
                    "/" #test_case_name,                                       \
              [](::benchmark::State& st) { func<a, b>(st, __VA_ARGS__); })))
#endif // BENCHMARK_TEMPLATE2_CAPTURE

// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define GEN(A, B, C, D)                                                        \
  BENCHMARK_TEMPLATE2_CAPTURE(BM, A, B, C, D)->Apply(CustomArguments)

#define GEN_T(A, C, D)                                                         \
  GEN(A, NoCoalescing, C, D);                                                  \
  GEN(A, CoalesceTo<uint16_t>, C, D);                                          \
  GEN(A, CoalesceTo<uint32_t>, C, D);                                          \
  GEN(A, CoalesceTo<uint64_t>, C, D)

GEN_T(BitstreamFlavorJPEG, Stuffed, true);
GEN_T(BitstreamFlavorJPEG, Unstuffed, false);
GEN_T(BitstreamFlavorMSB, Unstuffed, false);

// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace

} // namespace rawspeed

BENCHMARK_MAIN();
