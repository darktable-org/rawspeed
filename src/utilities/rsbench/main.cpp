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

#include "RawSpeed-API.h"        // for RawDecoder, Buffer, FileReader
#include <benchmark/benchmark.h> // for State, Benchmark, DoNotOptimize
#include <chrono>                // for duration, high_resolution_clock
#include <ctime>                 // for clock, clock_t
#include <map>                   // for map<>::mapped_type
#include <memory>                // for unique_ptr
#include <ratio>                 // for milli, ratio
#include <string>                // for string, to_string

#ifdef _OPENMP
#include <omp.h>
#endif

#define HAVE_STEADY_CLOCK

using rawspeed::CameraMetaData;
using rawspeed::FileReader;
using rawspeed::RawImage;
using rawspeed::RawParser;

namespace {

struct CPUClock {
  using rep = std::clock_t;
  using period = std::ratio<1, CLOCKS_PER_SEC>;
  using duration = std::chrono::duration<rep, period>;
  using time_point = std::chrono::time_point<CPUClock, duration>;

  // static constexpr bool is_steady = false;

  static time_point now() noexcept {
    return time_point{duration{std::clock()}};
  }
};

#if defined(HAVE_STEADY_CLOCK)
template <bool HighResIsSteady = std::chrono::high_resolution_clock::is_steady>
struct ChooseSteadyClock {
  using type = std::chrono::high_resolution_clock;
};

template <> struct ChooseSteadyClock<false> {
  using type = std::chrono::steady_clock;
};
#endif

struct ChooseClockType {
#if defined(HAVE_STEADY_CLOCK)
  using type = ChooseSteadyClock<>::type;
#else
  using type = std::chrono::high_resolution_clock;
#endif
};

template <typename Clock, typename period = std::ratio<1, 1>> struct Timer {
  using rep = double;
  using duration = std::chrono::duration<rep, period>;

  mutable typename Clock::time_point start = Clock::now();

  duration operator()() const {
    duration elapsed = Clock::now() - start;
    start = Clock::now();
    return elapsed;
  }
};

} // namespace

static int currThreadCount;

extern "C" int __attribute__((pure)) rawspeed_get_number_of_processor_cores() {
  return currThreadCount;
}

static inline void BM_RawSpeed(benchmark::State& state, const char* fileName,
                               int threads) {
  currThreadCount = threads;

#ifdef HAVE_PUGIXML
  static const CameraMetaData metadata(CMAKE_SOURCE_DIR "/data/cameras.xml");
#else
  static const CameraMetaData metadata{};
#endif

  FileReader reader(fileName);
  const auto map(reader.readFile());

  Timer<ChooseClockType::type> WT;
  Timer<CPUClock> TT;

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

  std::string label("FileSize,KB=");
  label += std::to_string(map->getSize() / (1UL << 10UL));
  state.SetLabel(label.c_str());

  const auto WallTime = WT();
  const auto TotalTime = TT();
  const auto ThreadingFactor = TotalTime.count() / WallTime.count();

  state.counters["CPUTime,s"] = TotalTime.count();
  state.counters["ThreadingFactor"] = ThreadingFactor;

  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(state.iterations() * map->getSize());
}

static void addBench(const char* fName, std::string tName, int threads) {
  tName += std::to_string(threads);

  auto* b =
      benchmark::RegisterBenchmark(tName.c_str(), &BM_RawSpeed, fName, threads);
  b->Unit(benchmark::kMillisecond);
  b->UseRealTime();
}

int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);

  auto hasFlag = [argc, argv](std::string flag) {
    bool found = false;
    for (int i = 1; i < argc; ++i) {
      if (!argv[i] || argv[i] != flag)
        continue;
      found = true;
      argv[i] = nullptr;
    }
    return found;
  };

  bool threading = hasFlag("-t");

#ifdef _OPENMP
  const auto threadsMax = omp_get_num_procs();
#else
  const auto threadsMax = 1;
#endif

  const auto threadsMin = threading ? 1 : threadsMax;

  for (int i = 1; i < argc; i++) {
    if (!argv[i])
      continue;

    const char* fName = argv[i];
    std::string tName(fName);
    tName += "/threads:";

    for (auto threads = threadsMin; threads <= threadsMax; threads++)
      addBench(fName, tName, threads);
  }

  benchmark::RunSpecifiedBenchmarks();
}
