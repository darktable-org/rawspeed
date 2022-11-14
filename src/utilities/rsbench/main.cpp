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

#include "RawSpeed-API.h"        // for RawDecoder, Buffer, FileReader, HAV...
#include "common/ChecksumFile.h" // for ChecksumFileEntry, ReadChecksumFile
#include <algorithm>             // for max
#include <benchmark/benchmark.h> // for Counter, Counter::Flags, Counter::k...
#include <chrono>                // for duration, high_resolution_clock
#include <ctime>                 // for clock, clock_t, CLOCKS_PER_SEC
#include <memory>                // for unique_ptr, allocator
#include <ratio>                 // for ratio
#include <string>                // for string, operator!=, to_string
#include <utility>               // for move
#include <vector>                // for vector

#ifdef HAVE_OPENMP
#include <omp.h> // for omp_get_max_threads
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

// Lazy cache for the referenced file's content - not actually read until
// requested the first time.
struct Entry {
  rawspeed::ChecksumFileEntry Name;
  std::unique_ptr<const rawspeed::Buffer> Content;

  const rawspeed::Buffer& getFileContents() {
    if (Content)
      return *Content;

    Content = FileReader(Name.FullFileName.c_str()).readFile();
    return *Content;
  }
};

static int currThreadCount;

extern "C" int __attribute__((pure)) rawspeed_get_number_of_processor_cores() {
  return currThreadCount;
}

static inline void BM_RawSpeed(benchmark::State& state, Entry* entry,
                               int threads) {
  currThreadCount = threads;

#ifdef HAVE_PUGIXML
  static const CameraMetaData metadata(RAWSPEED_SOURCE_DIR "/data/cameras.xml");
#else
  static const CameraMetaData metadata{};
#endif

  Timer<ChooseClockType::type> WT;
  Timer<CPUClock> TT;

  unsigned pixels = 0;
  for (auto _ : state) {
    RawParser parser(entry->getFileContents());
    auto decoder(parser.getDecoder(&metadata));

    decoder->failOnUnknown = false;
    decoder->checkSupport(&metadata);

    decoder->decodeRaw();
    decoder->decodeMetaData(&metadata);
    RawImage raw = decoder->mRaw;

    benchmark::DoNotOptimize(raw);

    pixels = raw->getUncroppedDim().area();
  }

  // These are total over all the `state.iterations()` iterations.
  const double CPUTime = TT().count();
  const double WallTime = WT().count();

  // For each iteration:
  state.counters.insert({
      {"CPUTime,s",
       benchmark::Counter(CPUTime, benchmark::Counter::Flags::kAvgIterations)},
      {"WallTime,s",
       benchmark::Counter(WallTime, benchmark::Counter::Flags::kAvgIterations)},
      {"CPUTime/WallTime", CPUTime / WallTime}, // 'Threading factor'
      {"Pixels", pixels},
      {"Pixels/CPUTime",
       benchmark::Counter(pixels / CPUTime,
                          benchmark::Counter::Flags::kIsIterationInvariant)},
      {"Pixels/WallTime",
       benchmark::Counter(pixels / WallTime,
                          benchmark::Counter::Flags::kIsIterationInvariant)},
      /* {"Raws", 1}, */
      {"Raws/CPUTime",
       benchmark::Counter(1.0 / CPUTime,
                          benchmark::Counter::Flags::kIsIterationInvariant)},
      {"Raws/WallTime",
       benchmark::Counter(1.0 / WallTime,
                          benchmark::Counter::Flags::kIsIterationInvariant)},
  });
  // Could also have counters wrt. the filesize,
  // but i'm not sure they are interesting.
}

static void addBench(Entry* entry, std::string tName, int threads) {
  tName += std::to_string(threads);

  auto* b =
      benchmark::RegisterBenchmark(tName.c_str(), &BM_RawSpeed, entry, threads);
  b->Unit(benchmark::kMillisecond);
  b->UseRealTime();
  b->MeasureProcessCPUTime();
}

int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);

  auto hasFlag = [argc, argv](const std::string& flag) {
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

#ifdef HAVE_OPENMP
  const auto threadsMax = omp_get_max_threads();
#else
  const auto threadsMax = 1;
#endif

  const auto threadsMin = threading ? 1 : threadsMax;

  // Were we told to use the repo (i.e. filelist.sha1 in that directory)?
  int useChecksumFile = hasFlag("-r");
  std::vector<Entry> Worklist;
  if (useChecksumFile && useChecksumFile + 1 < argc) {
    char*& checksumFileRepo = argv[useChecksumFile + 1];
    if (checksumFileRepo) {
      const auto readEntries = rawspeed::ReadChecksumFile(checksumFileRepo);
      Worklist.reserve(readEntries.size());
      for (const rawspeed::ChecksumFileEntry& entryName : readEntries) {
        Entry Entry;
        Entry.Name = entryName;
        Worklist.emplace_back(std::move(Entry));
      }
    }
    checksumFileRepo = nullptr;
  }

  // If there are normal filenames, append them.
  for (int i = 1; i < argc; i++) {
    if (!argv[i])
      continue;

    Entry Entry;
    const char* fName = argv[i];
    // These are supposed to be either absolute paths, or relative the run dir.
    // We don't do any beautification.
    Entry.Name.FullFileName = fName;
    Entry.Name.RelFileName = fName;
    Worklist.emplace_back(std::move(Entry));
  }

  // And finally, actually add the raws to be benchmarked.
  for (Entry& entry : Worklist) {
    std::string tName(entry.Name.RelFileName);
    tName += "/threads:";

    for (auto threads = threadsMin; threads <= threadsMax; threads++)
      addBench(&entry, tName, threads);
  }

  benchmark::RunSpecifiedBenchmarks();
}
