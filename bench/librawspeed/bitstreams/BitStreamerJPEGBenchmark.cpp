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
#include "adt/Casts.h"
#include "adt/DefaultInitAllocatorAdaptor.h"
#include "adt/Invariant.h"
#include "adt/Optional.h"
#include "bench/Common.h"
#include "bitstreams/BitStreamerMSB.h"
#include "common/Common.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <random>
#include <vector>
#include <benchmark/benchmark.h>

#ifndef NDEBUG
#include <limits>
#endif

namespace rawspeed {

namespace {

// i'th element is the frequency with which
// the byte `i` is found in an average JPEG byte stream.
//
// NOTE: that `a[0xFF]` means you need to refer to the next table!
constexpr std::array<uint64_t, 256> ByteFrequency = {
    10493257, 10784784, 11552191, 11893555, 12613144, 13142898, 12997374,
    13899275, 12767738, 13768690, 14067759, 14319807, 14298384, 14549654,
    14787598, 16204120, 12564638, 14704327, 15132777, 14772038, 14721008,
    16318288, 16165051, 16084372, 14589355, 15975372, 15789517, 16176233,
    17103573, 16389861, 17178739, 19566197, 12966690, 14939127, 15610587,
    16752954, 17041360, 16836032, 16076146, 17059902, 14578757, 16091877,
    16727621, 18080916, 17798565, 17160354, 16884990, 18016216, 13728953,
    16699694, 17247145, 16535935, 15410093, 17790033, 17505607, 17062629,
    16204122, 18729168, 17299562, 16769498, 18232031, 18145938, 19781670,
    21534495, 11331827, 14291644, 14185576, 15783900, 14628539, 16752046,
    16118713, 18395146, 15623136, 17719214, 16530868, 17731977, 15769435,
    17046218, 16765658, 18737070, 13228107, 16456840, 16329879, 16995821,
    15738285, 18833989, 18575650, 19748780, 17437816, 19175677, 17737907,
    18686908, 17947791, 18231973, 18445785, 21014291, 13778765, 16390784,
    16945784, 18735616, 17800539, 18504439, 17162043, 18246945, 14580463,
    17138374, 17246435, 19567468, 18000143, 18165063, 17059260, 18333885,
    14699997, 18157635, 18494395, 18681789, 16039637, 18755071, 17705487,
    16502854, 16349978, 19475294, 18135626, 18404224, 19367350, 24366361,
    20257637, 22731340, 10784899, 12672656, 14185602, 15004113, 13933378,
    15252018, 15855022, 17094546, 14503399, 16136927, 16968789, 17924243,
    16261588, 17408400, 18680524, 20524483, 15332344, 17670025, 18742651,
    18371102, 15933041, 18490712, 18796265, 18818308, 15835711, 17808890,
    17416555, 18385523, 17848445, 17685419, 19183656, 21453184, 12650093,
    15036791, 15765357, 17764334, 16308312, 17422027, 16753335, 18445554,
    15113106, 17233138, 17846593, 20237681, 18808801, 19265405, 19304091,
    21445005, 16435437, 18992248, 19056275, 18871173, 16314514, 19007013,
    18667634, 18326497, 16647767, 18446563, 17500274, 17437242, 17638057,
    18434446, 24246779, 21500863, 12125753, 14904989, 14975537, 17166842,
    16018526, 18142274, 17551177, 20836099, 17376424, 19394242, 17909585,
    19873129, 17863152, 18763409, 18746276, 21901307, 14468289, 17073465,
    17390849, 18194814, 16602085, 19238700, 19504566, 21024483, 17986684,
    18759080, 17590236, 18305128, 17151973, 16704918, 17577197, 24350469,
    13246295, 15768268, 17229315, 19654834, 18978722, 19267508, 19459062,
    22426506, 16951164, 18450043, 18602860, 20972912, 18743890, 17732170,
    16789701, 23607383, 14314573, 18700611, 19757997, 23215145, 19361294,
    20804837, 18763614, 23882738, 16663983, 23464670, 22040143, 24327009,
    20768026, 21851961, 22533160, 22588801};

// i'th element is the frequency with which a sequence `0xFF00` consecutively
// repeated `i` times is found in an average JPEG byte stream.
constexpr std::array<uint64_t, 4> NumConsecutive0xFF00Frequency = {0, 22513031,
                                                                   75445, 325};

struct JPEGStuffedByteStreamGenerator final {
  std::vector<uint8_t,
              DefaultInitAllocatorAdaptor<uint8_t, std::allocator<uint8_t>>>
      dataStorage;
  int64_t numBytesGenerated;

  [[nodiscard]] Array1DRef<const uint8_t> getInput() const {
    return {dataStorage.data(), implicit_cast<int>(dataStorage.size())};
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wframe-larger-than="
#pragma GCC diagnostic ignored "-Wstack-usage="
  __attribute__((noinline)) explicit JPEGStuffedByteStreamGenerator(
      const int64_t numBytesMax) {
    invariant(numBytesMax > 0);
    const auto expectedOverhead = roundUpDivision(numBytesMax, 100); // <=1%
    dataStorage.reserve(implicit_cast<size_t>(numBytesMax + expectedOverhead));

    // Here we only need to differentiate between a normal byte,
    // and an 0xFF00 sequence, so clump together non-0xFF frequencies.
    // This makes distribution sampling -40% faster.
    constexpr uint64_t TotalWeight = std::accumulate(
        ByteFrequency.begin(), ByteFrequency.end(), uint64_t(0));
    constexpr uint64_t ControlSequenceStartWeight = ByteFrequency.back();

    std::bernoulli_distribution controlSequenceStartDistribution(
        implicit_cast<double>(ControlSequenceStartWeight) /
        implicit_cast<double>(TotalWeight));
    std::discrete_distribution<uint8_t> numConsecutive0xFF00Distribution(
        NumConsecutive0xFF00Frequency.begin(),
        NumConsecutive0xFF00Frequency.end());

    std::random_device rd;
    std::mt19937_64 gen(rd());

    for (numBytesGenerated = 0; numBytesGenerated < numBytesMax;) {
      bool isNormalByte = !controlSequenceStartDistribution(gen);
      if (isNormalByte) {
        dataStorage.emplace_back(0x00);
        ++numBytesGenerated;
      } else {
        const int len = numConsecutive0xFF00Distribution(gen);
        invariant(len > 0);
        for (int i = 0; i != len; ++i) {
          dataStorage.emplace_back(0xFF);
          dataStorage.emplace_back(0x00); // This is a no-op stuffing byte.
        }
        numBytesGenerated += len;
      }
    }
    invariant(numBytesGenerated >= numBytesMax);
  }
#pragma GCC diagnostic pop
};

struct JPEGUnstuffedByteStreamGenerator final {
  std::vector<uint8_t,
              DefaultInitAllocatorAdaptor<uint8_t, std::allocator<uint8_t>>>
      dataStorage;
  int64_t numBytesGenerated;

  [[nodiscard]] Array1DRef<const uint8_t> getInput() const {
    return {dataStorage.data(), implicit_cast<int>(dataStorage.size())};
  }

  __attribute__((noinline)) explicit JPEGUnstuffedByteStreamGenerator(
      const int64_t numBytesMax)
      : numBytesGenerated(numBytesMax) {
    invariant(numBytesGenerated > 0);
    dataStorage.resize(implicit_cast<size_t>(numBytesGenerated), 0x00);
  }
};

template <typename T> void BM(benchmark::State& state, bool Stuffed) {
  int64_t numBytes = state.range(0);
  assert(numBytes > 0);
  assert(numBytes <= std::numeric_limits<int>::max());

  Optional<JPEGStuffedByteStreamGenerator> genStuffed;
  Optional<JPEGUnstuffedByteStreamGenerator> genUnstuffed;
  Optional<Array1DRef<const uint8_t>> input;
  if (Stuffed) {
    genStuffed.emplace(numBytes);
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

  // NOLINTNEXTLINE(readability-simplify-boolean-expr)
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
