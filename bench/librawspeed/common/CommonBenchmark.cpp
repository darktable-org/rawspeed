/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2023 Roman Lebedev

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

#include "bench/Common.h"
#include "adt/Point.h"
#include "common/Common.h"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <benchmark/benchmark.h>

using rawspeed::copyPixels;
using rawspeed::iPoint2D;

namespace {

void BM_CopyPixels(benchmark::State& state, bool padddedRows) {
  iPoint2D dims = areaToRectangle(state.range(0), {3, 2});

  const int width = dims.x;
  const int height = dims.y;

  int dstPitch = dims.x;
  int srcPitch = dims.x;

  if (padddedRows) {
    dstPitch *= 2;
    srcPitch *= 2;
  }

  std::vector<uint8_t> dst(static_cast<size_t>(dstPitch) * height, uint8_t(0));
  std::vector<uint8_t> src(static_cast<size_t>(srcPitch) * height, uint8_t(0));

  for (auto _ : state) {
    copyPixels(reinterpret_cast<std::byte*>(&(dst[0])), dstPitch,
               reinterpret_cast<const std::byte*>(&(src[0])), srcPitch, width,
               height);
  }

  state.SetComplexityN(dims.area());
  state.counters.insert(
      {{"Bytes",
        benchmark::Counter(sizeof(uint8_t) * dims.area(),
                           benchmark::Counter::Flags::kIsIterationInvariantRate,
                           benchmark::Counter::kIs1024)}});
}

void BM_CopyPixels2DContiguous(benchmark::State& state) {
  BM_CopyPixels(state, /*padddedRows=*/false);
}

void BM_CopyPixels2DStrided(benchmark::State& state) {
  BM_CopyPixels(state, /*padddedRows=*/true);
}

inline void CustomArguments(benchmark::internal::Benchmark* b) {
  b->MeasureProcessCPUTime();
  b->UseRealTime();
  b->Unit(benchmark::kMicrosecond);

  static constexpr int L2dByteSize = 512U * (1U << 10U);
  static constexpr int L2dNPixels = L2dByteSize / 2;
  static constexpr int MaxPixelsOptimal = (1 << 5) * L2dNPixels;

  if (benchmarkDryRun()) {
    b->Arg(L2dNPixels);
    return;
  }

  b->RangeMultiplier(2);
  if constexpr ((false)) {
    b->Arg(MaxPixelsOptimal);
  } else {
    b->Range(1, 2 * MaxPixelsOptimal)->Complexity(benchmark::oN);
  }
}

BENCHMARK(BM_CopyPixels2DContiguous)->Apply(CustomArguments);
BENCHMARK(BM_CopyPixels2DStrided)->Apply(CustomArguments);

} // namespace

BENCHMARK_MAIN();
