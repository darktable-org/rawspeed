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

#include "adt/Bit.h"
#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "adt/CoalescingOutputIterator.h"
#include "adt/DefaultInitAllocatorAdaptor.h"
#include "adt/Invariant.h"
#include "adt/PartitioningOutputIterator.h"
#include "bench/Common.h"
#include "bitstreams/BitVacuumerLSB.h"
#include "bitstreams/BitVacuumerMSB.h"
#include "bitstreams/BitVacuumerMSB16.h"
#include "bitstreams/BitVacuumerMSB32.h"
#include "common/Common.h"
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <random>
#include <type_traits>
#include <vector>
#include <benchmark/benchmark.h>

#ifndef NDEBUG
#include <limits>
#endif

namespace rawspeed {

namespace {

struct BitstreamFlavorLSB;
struct BitstreamFlavorMSB;
struct BitstreamFlavorMSB16;
struct BitstreamFlavorMSB32;

template <typename T> struct BitStreamRoundtripTypes final {};

template <> struct BitStreamRoundtripTypes<BitstreamFlavorLSB> final {
  template <typename OutputIterator>
  using vacuumer = BitVacuumerLSB<OutputIterator>;
};

template <> struct BitStreamRoundtripTypes<BitstreamFlavorMSB> final {
  template <typename OutputIterator>
  using vacuumer = BitVacuumerMSB<OutputIterator>;
};

template <> struct BitStreamRoundtripTypes<BitstreamFlavorMSB16> final {
  template <typename OutputIterator>
  using vacuumer = BitVacuumerMSB16<OutputIterator>;
};

template <> struct BitStreamRoundtripTypes<BitstreamFlavorMSB32> final {
  template <typename OutputIterator>
  using vacuumer = BitVacuumerMSB32<OutputIterator>;
};

struct BitVectorLengthsGenerator final {
  std::vector<int8_t,
              DefaultInitAllocatorAdaptor<int8_t, std::allocator<int8_t>>>
      dataStorage;

  int64_t numBitsToProduce = 0;

  [[nodiscard]] Array1DRef<const int8_t> getInput() const {
    return {dataStorage.data(), implicit_cast<int>(dataStorage.size())};
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wframe-larger-than="
#pragma GCC diagnostic ignored "-Wstack-usage="
  __attribute__((noinline)) explicit BitVectorLengthsGenerator(
      const int64_t maxBytes) {
    invariant(maxBytes > 0);

    std::uniform_int_distribution<> dist(0, 32);

    std::random_device rd;
    std::mt19937_64 gen(rd());

    for (int64_t numBits = 0; implicit_cast<int64_t>(roundUpDivisionSafe(
                                  numBits, CHAR_BIT)) < maxBytes;) {
      int len = dist(gen);
      numBitsToProduce += len;
      dataStorage.emplace_back(len);
      numBits += bitwidth<int8_t>();
      numBits += len;
    }
  }
#pragma GCC diagnostic pop
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

template <typename T, typename C> void BM(benchmark::State& state) {
  int64_t numBytes = state.range(0);
  assert(numBytes > 0);
  assert(numBytes <= std::numeric_limits<int>::max());

  const BitVectorLengthsGenerator gen(numBytes);
  const Array1DRef<const int8_t> input = gen.getInput();
  benchmark::DoNotOptimize(input.begin());

  using OutputChunkType = typename C::value_type;
  std::vector<OutputChunkType,
              DefaultInitAllocatorAdaptor<OutputChunkType,
                                          std::allocator<OutputChunkType>>>
      output;
  output.reserve(implicit_cast<size_t>(roundUpDivisionSafe(
      gen.numBitsToProduce, CHAR_BIT * sizeof(OutputChunkType))));

  for (auto _ : state) {
    output.clear();

    auto bsInserter = PartitioningOutputIterator(
        getMaybeCoalescingOutputIterator<C::coalescing::value>(
            std::back_inserter(output)));
    using BitVacuumer = typename BitStreamRoundtripTypes<T>::template vacuumer<
        decltype(bsInserter)>;
    auto bv = BitVacuumer(bsInserter);

    int bits = 0;
    for (const auto& len : input) {
      benchmark::DoNotOptimize(bits);
      bv.put(bits, len);
    }
  }

  state.SetComplexityN(sizeof(typename decltype(output)::value_type) *
                       output.size());
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
  static constexpr int MaxBytesOptimal = L2dByteSize * (1U << 3);

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

#define GEN(A, B) BENCHMARK_TEMPLATE2(BM, A, B)->Apply(CustomArguments)

#define GEN_T(A)                                                               \
  GEN(A, NoCoalescing);                                                        \
  GEN(A, CoalesceTo<uint16_t>);                                                \
  GEN(A, CoalesceTo<uint32_t>);                                                \
  GEN(A, CoalesceTo<uint64_t>)

GEN_T(BitstreamFlavorLSB);
GEN_T(BitstreamFlavorMSB);
GEN_T(BitstreamFlavorMSB16);
GEN_T(BitstreamFlavorMSB32);

// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace

} // namespace rawspeed

BENCHMARK_MAIN();
