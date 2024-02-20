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
#include "adt/DefaultInitAllocatorAdaptor.h"
#include "adt/Invariant.h"
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

    for (int64_t numBits = 0; implicit_cast<int64_t>(roundUpDivision(
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

template <typename T> void BM(benchmark::State& state) {
  int64_t numBytes = state.range(0);
  assert(numBytes > 0);
  assert(numBytes <= std::numeric_limits<int>::max());

  const BitVectorLengthsGenerator gen(numBytes);
  const Array1DRef<const int8_t> input = gen.getInput();
  benchmark::DoNotOptimize(input.begin());

  std::vector<uint8_t,
              DefaultInitAllocatorAdaptor<uint8_t, std::allocator<uint8_t>>>
      output;
  output.reserve(
      implicit_cast<size_t>(roundUpDivision(gen.numBitsToProduce, CHAR_BIT)));

  for (auto _ : state) {
    output.clear();

    auto bsInserter = std::back_inserter(output);
    using BitVacuumer = typename BitStreamRoundtripTypes<T>::template vacuumer<
        decltype(bsInserter)>;
    auto bv = BitVacuumer(bsInserter);

    int bits = 0;
    for (const auto& len : input) {
      benchmark::DoNotOptimize(bits);
      bv.put(bits, len);
    }
  }

  state.SetComplexityN(sizeof(decltype(output)::value_type) * output.size());
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
  b->Unit(benchmark::kMillisecond);
  b->RangeMultiplier(2);

  static constexpr int L1dByteSize = 32U * (1U << 10U);
  static constexpr int L2dByteSize = 512U * (1U << 10U);
  static constexpr int MaxBytesOptimal = L2dByteSize * (1U << 3);

  if (benchmarkDryRun()) {
    b->Arg(L1dByteSize);
    return;
  }

  // NOLINTNEXTLINE(readability-simplify-boolean-expr)
  if constexpr ((true)) {
    b->Arg(MaxBytesOptimal);
  } else {
    b->Range(8, MaxBytesOptimal * (1U << 2));
    b->Complexity(benchmark::oN);
  }
}

BENCHMARK_TEMPLATE(BM, BitstreamFlavorLSB)->Apply(CustomArguments);
BENCHMARK_TEMPLATE(BM, BitstreamFlavorMSB)->Apply(CustomArguments);
BENCHMARK_TEMPLATE(BM, BitstreamFlavorMSB16)->Apply(CustomArguments);
BENCHMARK_TEMPLATE(BM, BitstreamFlavorMSB32)->Apply(CustomArguments);

} // namespace

} // namespace rawspeed

BENCHMARK_MAIN();
