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

#include "RawSpeed-API.h"        // for RawDecoder, Buffer, FileReader, Raw...
#include "common/ChecksumFile.h" // for ParseChecksumFile
#include <benchmark/benchmark.h> // for Counter, State, DoNotOptimize, Init...
#include <chrono>                // for duration, high_resolution_clock
#include <ctime>                 // for clock, clock_t
#include <memory>                // for unique_ptr
#include <ratio>                 // for ratio
#include <string>                // for string, to_string, operator!=
#include <sys/time.h>            // for CLOCKS_PER_SEC

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

  unsigned pixels = 0;
  for (auto _ : state) {
    RawParser parser(map.get());
    auto decoder(parser.getDecoder(&metadata));

    decoder->failOnUnknown = false;
    decoder->checkSupport(&metadata);

    decoder->decodeRaw();
    decoder->decodeMetaData(&metadata);
    RawImage raw = decoder->mRaw;

    benchmark::DoNotOptimize(raw);

    pixels = raw->getUncroppedDim().area();
  }

  std::string label("FileSize,MB=");
  label += std::to_string(double(map->getSize()) / double(1UL << 20UL));
  label += "; MPix=";
  label += std::to_string(double(pixels) / 1e+06);
  state.SetLabel(label.c_str());

  const auto WallTime = WT();
  const auto TotalTime = TT();
  const auto ThreadingFactor = TotalTime.count() / WallTime.count();

  state.counters.insert({
      {"Pixels", benchmark::Counter(state.iterations() * double(pixels),
                                    benchmark::Counter::kIsRate)},
      {"CPUTime,s", TotalTime.count() / state.iterations()},
      {"ThreadingFactor", ThreadingFactor},
  });

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
    int found = 0;
    for (int i = 1; i < argc; ++i) {
      if (!argv[i] || argv[i] != flag)
        continue;
      found = i;
      argv[i] = nullptr;
    }
    return found;
  };

  bool threading = hasFlag("-t");

#ifdef _OPENMP
  const auto threadsMax = omp_get_max_threads();
#else
  const auto threadsMax = 1;
#endif

  const auto threadsMin = threading ? 1 : threadsMax;

  // Were we told to use the repo (i.e. filelist.sha1 in that directory)?
  int useChecksumFile = hasFlag("-r");
  std::vector<rawspeed::ChecksumFileEntry> ChecksumFileEntries;
  if (useChecksumFile && useChecksumFile + 1 < argc) {
    char*& checksumFileRepo = argv[useChecksumFile + 1];
    if (checksumFileRepo)
      ChecksumFileEntries = rawspeed::ReadChecksumFile(checksumFileRepo);
    checksumFileRepo = nullptr;
  }

  // If there are normal filenames, append them.
  for (int i = 1; i < argc; i++) {
    if (!argv[i])
      continue;

    rawspeed::ChecksumFileEntry Entry;
    const char* fName = argv[i];
    // These are supposed to be either absolute paths, or relative the run dir.
    // We don't do any beautification.
    Entry.FullFileName = fName;
    Entry.RelFileName = fName;
    ChecksumFileEntries.emplace_back(Entry);
  }

  // And finally, actually add the raws to be benchmarked.
  for (const auto& Entry : ChecksumFileEntries) {
    const char* fName = Entry.RelFileName.c_str();
    std::string tName(fName);
    tName += "/threads:";

    for (auto threads = threadsMin; threads <= threadsMax; threads++)
      addBench(Entry.FullFileName.c_str(), tName, threads);
  }

  benchmark::RunSpecifiedBenchmarks();
}
