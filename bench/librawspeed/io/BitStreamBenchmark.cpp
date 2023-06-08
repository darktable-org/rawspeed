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

#include "io/BitStream.h"        // for BitStream
#include "bench/Common.h"        // for benchmarkDryRun
#include "io/BitPumpJPEG.h"      // for BitPumpJPEG, BitStream<>::fillCache
#include "io/BitPumpLSB.h"       // for BitPumpLSB, BitStream<>::fillCache
#include "io/BitPumpMSB.h"       // for BitPumpMSB, BitStream<>::fillCache
#include "io/BitPumpMSB16.h"     // for BitPumpMSB16, BitStream<>::fillCache
#include "io/BitPumpMSB32.h"     // for BitPumpMSB32, BitStream<>::fillCache
#include "io/Buffer.h"           // for Buffer, DataBuffer
#include "io/ByteStream.h"       // for ByteStream
#include "io/Endianness.h"       // for Endianness, Endianness::unknown
#include <algorithm>             // for fill_n
#include <cassert>               // for assert
#include <cstddef>               // for size_t
#include <cstdint>               // for uint8_t
#include <string>                // for string, to_string, allocator
#include <vector>                // for vector
#include <benchmark/benchmark.h> // for State, Benchmark, Initialize, RunSp...

using rawspeed::BitPumpJPEG;
using rawspeed::BitPumpLSB;
using rawspeed::BitPumpMSB;
using rawspeed::BitPumpMSB16;
using rawspeed::BitPumpMSB32;
using rawspeed::Endianness;

static constexpr const size_t STEP_MAX = 32;

template <typename Pump>
static inline void BM_BitStream(benchmark::State& state, unsigned int fillSize,
                                unsigned int Step) {
  assert(state.range(0) > 0);
  assert((size_t)state.range(0) <=
         std::numeric_limits<rawspeed::Buffer::size_type>::max());

  assert(fillSize > 0);
  assert(fillSize <= STEP_MAX);

  assert(Step > 0);
  assert(Step <= STEP_MAX);

  assert(Step <= fillSize);

  assert((Step == 1) || rawspeed::isAligned(Step, 2));
  assert((fillSize == 1) || rawspeed::isAligned(fillSize, 2));

  const std::vector<uint8_t> storage(state.range(0));
  const rawspeed::Buffer b(storage.data(), state.range(0));
  assert(b.getSize() > 0);
  assert(b.getSize() == (size_t)state.range(0));

  const rawspeed::DataBuffer db(b, Endianness::unknown);
  const rawspeed::ByteStream bs(db);

  size_t processedBits = 0;
  for (auto _ : state) {
    Pump pump(bs);

    for (processedBits = 0; processedBits <= 8 * b.getSize();) {
      pump.fill(fillSize);

      // NOTE: you may want to change the callee here
      for (auto i = 0U; i < fillSize; i += Step)
        pump.skipBitsNoFill(Step);

      processedBits += fillSize;
    }
  }

  assert(processedBits > fillSize);
  processedBits -= fillSize;

  assert(rawspeed::roundUp(8 * b.getSize(), fillSize) == processedBits);

  state.SetComplexityN(processedBits / 8);
  state.SetItemsProcessed(processedBits * state.iterations());
  state.SetBytesProcessed(state.items_processed() / 8);
}

static inline void CustomArguments(benchmark::internal::Benchmark* b) {
  if (benchmarkDryRun()) {
    b->Arg((512U * (1U << 10U)) / 10);
    return;
  }

  b->RangeMultiplier(2);
#if 1
  b->Arg(256 << 20);
#else
  b->Range(1, 1024 << 20);
  b->Complexity(benchmark::oN);
#endif
  b->Unit(benchmark::kMillisecond);
}

template <typename PUMP> void registerPump(const char* pumpName) {
  for (size_t i = 1; i <= STEP_MAX; i *= 2) {
    for (size_t j = 1; j <= i && j <= STEP_MAX; j *= 2) {
      std::string name("BM_BitStream<");
      name += "Spec<";
      name += pumpName;
      name += ">, Fill<";
      name += std::to_string(i);
      name += ">, Step<";
      name += std::to_string(j);
      name += ">>";

      const auto Fn = BM_BitStream<PUMP>;
      auto* b = benchmark::RegisterBenchmark(name, Fn, i, j);
      b->Apply(CustomArguments);
    }
  }
}

#define REGISTER_PUMP(PUMP) registerPump<PUMP>(#PUMP)

int main(int argc, char** argv) {
  REGISTER_PUMP(BitPumpLSB);
  REGISTER_PUMP(BitPumpMSB);
  REGISTER_PUMP(BitPumpMSB16);
  REGISTER_PUMP(BitPumpMSB32);
  REGISTER_PUMP(BitPumpJPEG);

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
}
