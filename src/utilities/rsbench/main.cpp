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

#include "RawSpeed-API.h"            // for RawDecoder, Buffer, FileReader
#include <benchmark/benchmark_api.h> // for State, Benchmark, DoNotOptimize
#include <ctime>                     // for clock, clock_t, CLOCKS_PER_SEC
#include <memory>                    // for unique_ptr
#include <string>                    // for string, to_string
// IWYU pragma: no_include <sys/time.h>

using rawspeed::CameraMetaData;
using rawspeed::FileReader;
using rawspeed::RawImage;
using rawspeed::RawParser;

static inline void BM_RawSpeed(benchmark::State& state, const char* fileName) {
#ifdef HAVE_PUGIXML
  static const CameraMetaData metadata(CMAKE_SOURCE_DIR "/data/cameras.xml");
#else
  static const CameraMetaData metadata{};
#endif

  FileReader reader(fileName);
  const auto map(reader.readFile());

  const std::clock_t c_start = std::clock();

  while (state.KeepRunning()) {
    RawParser parser(map.get());
    auto decoder(parser.getDecoder(&metadata));

    decoder->failOnUnknown = false;
    decoder->checkSupport(&metadata);

    decoder->decodeRaw();
    decoder->decodeMetaData(&metadata);
    RawImage raw = decoder->mRaw;

    benchmark::DoNotOptimize(raw);
  }

  const std::clock_t c_end = std::clock();

  std::string label("filesize=");
  label += std::to_string(map->getSize() / (1UL << 10UL));
  label += "KB;CPU-seconds=";
  label += std::to_string(1000.0 * (c_end - c_start) / CLOCKS_PER_SEC);
  label += "ms";
  state.SetLabel(label.c_str());

  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(state.iterations() * map->getSize());
}

int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);

  for (int i = 1; i < argc; i++) {
    const char* fName = argv[i];
    auto* b = benchmark::RegisterBenchmark(fName, &BM_RawSpeed, fName);
    b->Unit(benchmark::kMillisecond);
    b->UseRealTime();
  }

  benchmark::RunSpecifiedBenchmarks();
}
