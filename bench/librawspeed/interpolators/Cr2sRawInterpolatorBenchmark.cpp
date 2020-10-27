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

#include "interpolators/Cr2sRawInterpolator.h" // for Cr2sRawInterpolator
#include "bench/Common.h"                      // for areaToRectangle
#include "common/Point.h"                      // for iPoint2D
#include "common/RawImage.h"                   // for RawImage, ImageMetaData
#include <array>                               // for array
#include <benchmark/benchmark.h>               // for State, Benchmark, BEN...
#include <cstdint>                             // for uint16_t
#include <type_traits>                         // for integral_constant

using rawspeed::Cr2sRawInterpolator;
using rawspeed::iPoint2D;
using rawspeed::roundUp;
using rawspeed::RawImage;
using rawspeed::TYPE_USHORT16;
using std::array;
using std::integral_constant;

template <int N> using v = integral_constant<int, N>;

template <const iPoint2D& subSampling, typename version>
static inline void BM_Cr2sRawInterpolator(benchmark::State& state) {
  static const array<int, 3> sraw_coeffs = {{999, 1000, 1001}};
  static const int hue = 1269;

  iPoint2D interpolatedDims = areaToRectangle(state.range(0), {3, 2});

  interpolatedDims.x = roundUp(interpolatedDims.x, 6);
  if (subSampling.y == 2)
    interpolatedDims.x = roundUp(interpolatedDims.x, 4);

  iPoint2D subsampledDim = interpolatedDims;
  subsampledDim.x /= subSampling.x;
  subsampledDim.y /= subSampling.y;
  subsampledDim.x *= 2 + subSampling.x * subSampling.y;

  RawImage subsampledRaw = RawImage::create(subsampledDim, TYPE_USHORT16, 1);
  subsampledRaw->metadata.subsampling = subSampling;

  RawImage mRaw = RawImage::create(interpolatedDims, TYPE_USHORT16, 3);
  mRaw->metadata.subsampling = subSampling;

  Cr2sRawInterpolator i(mRaw, subsampledRaw->getU16DataAsUncroppedArray2DRef(),
                        sraw_coeffs, hue);

  for (auto _ : state)
    i.interpolate(version::value);

  state.SetComplexityN(interpolatedDims.area());
  state.counters.insert(
      {{"Pixels", benchmark::Counter(
                      state.complexity_length_n(),
                      benchmark::Counter::Flags::kIsIterationInvariantRate)},
       {"Bytes",
        benchmark::Counter(3UL * sizeof(uint16_t) * state.complexity_length_n(),
                           benchmark::Counter::Flags::kIsIterationInvariantRate,
                           benchmark::Counter::kIs1024)}});
}

static inline void CustomArguments(benchmark::internal::Benchmark* b) {
  b->MeasureProcessCPUTime();
  b->UseRealTime();
  b->RangeMultiplier(2);
#if 1
  b->Arg(2 * 3 * 2 * 1'000'000);
#else
  b->Range(1, 256 << 20)->Complexity(benchmark::oN);
#endif
  b->Unit(benchmark::kMillisecond);
}

static constexpr const iPoint2D S422(2, 1);
BENCHMARK_TEMPLATE(BM_Cr2sRawInterpolator, S422, v<0>)->Apply(CustomArguments);
BENCHMARK_TEMPLATE(BM_Cr2sRawInterpolator, S422, v<1>)->Apply(CustomArguments);
BENCHMARK_TEMPLATE(BM_Cr2sRawInterpolator, S422, v<2>)->Apply(CustomArguments);

static constexpr const iPoint2D S420(2, 2);
BENCHMARK_TEMPLATE(BM_Cr2sRawInterpolator, S420, v<1>)->Apply(CustomArguments);
BENCHMARK_TEMPLATE(BM_Cr2sRawInterpolator, S420, v<2>)->Apply(CustomArguments);

BENCHMARK_MAIN();
