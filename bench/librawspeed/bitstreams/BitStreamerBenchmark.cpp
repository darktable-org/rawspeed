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

#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "bench/Common.h"
#include "bitstreams/BitStreamerJPEG.h"
#include "bitstreams/BitStreamerLSB.h"
#include "bitstreams/BitStreamerMSB.h"
#include "bitstreams/BitStreamerMSB16.h"
#include "bitstreams/BitStreamerMSB32.h"
#include <cassert>
#include <cstddef>
#include <benchmark/benchmark.h>

#ifndef NDEBUG
#include "io/Buffer.h"
#endif

#ifndef DEBUG
#include <cstdint>
#include <string>
#include <vector>
#endif

#ifdef DEBUG
#include "common/Common.h"
#include <limits>
#endif

using rawspeed::BitStreamerJPEG;
using rawspeed::BitStreamerLSB;
using rawspeed::BitStreamerMSB;
using rawspeed::BitStreamerMSB16;
using rawspeed::BitStreamerMSB32;

namespace {

constexpr const int STEP_MAX = 32;

template <typename Pump>
inline void BM_BitStreamer(benchmark::State& state, int fillSize, int Step) {
  assert(state.range(0) > 0);
  assert(static_cast<size_t>(state.range(0)) <=
         std::numeric_limits<rawspeed::Buffer::size_type>::max());

  assert(fillSize > 0);
  assert(fillSize <= STEP_MAX);

  assert(Step > 0);
  assert(Step <= STEP_MAX);

  assert(Step <= fillSize);

  assert((Step == 1) || rawspeed::isAligned(Step, 2));
  assert((fillSize == 1) || rawspeed::isAligned(fillSize, 2));

  const std::vector<uint8_t> inputStorage(
      rawspeed::implicit_cast<size_t>(state.range(0)));
  const rawspeed::Array1DRef<const uint8_t> input(
      inputStorage.data(), rawspeed::implicit_cast<int>(state.range(0)));

  int processedBits = 0;
  for (auto _ : state) {
    Pump pump(input);

    for (processedBits = 0; processedBits <= 8 * input.size();) {
      pump.fill(fillSize);

      // NOTE: you may want to change the callee here
      for (auto i = 0; i < fillSize; i += Step)
        pump.skipBitsNoFill(Step);

      processedBits += fillSize;
    }
  }

  assert(processedBits > fillSize);
  processedBits -= fillSize;

  assert(rawspeed::roundUp(8 * input.size(), fillSize) ==
         rawspeed::implicit_cast<uint64_t>(processedBits));

  state.SetComplexityN(processedBits / 8);
  state.SetItemsProcessed(processedBits * state.iterations());
  state.SetBytesProcessed(state.items_processed() / 8);
}

inline void CustomArguments(benchmark::internal::Benchmark* b) {
  if (benchmarkDryRun()) {
    b->Arg((512U * (1U << 10U)) / 10);
    return;
  }

  b->RangeMultiplier(2);
  if constexpr ((true)) {
    b->Arg(256 << 20);
  } else {
    b->Range(1, 1024 << 20);
    b->Complexity(benchmark::oN);
  }
  b->Unit(benchmark::kMillisecond);
}

template <typename PUMP> void registerPump(const char* pumpName) {
  for (size_t i = 1; i <= STEP_MAX; i *= 2) {
    for (size_t j = 1; j <= i && j <= STEP_MAX; j *= 2) {
      std::string name("BM_BitStreamer<");
      name += "Spec<";
      name += pumpName;
      name += ">, Fill<";
      name += std::to_string(i);
      name += ">, Step<";
      name += std::to_string(j);
      name += ">>";

      const auto Fn = BM_BitStreamer<PUMP>;
      auto* b = benchmark::RegisterBenchmark(name, Fn, i, j);
      b->Apply(CustomArguments);
    }
  }
}

} // namespace

#define REGISTER_PUMP(PUMP) registerPump<PUMP>(#PUMP)

int main(int argc, char** argv) {
  REGISTER_PUMP(BitStreamerLSB);
  REGISTER_PUMP(BitStreamerMSB);
  REGISTER_PUMP(BitStreamerMSB16);
  REGISTER_PUMP(BitStreamerMSB32);
  REGISTER_PUMP(BitStreamerJPEG);

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
}
