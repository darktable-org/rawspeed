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

#include "RawSpeed-API.h"
#include "adt/AlignedAllocator.h"
#include "adt/Array1DRef.h"
#include "adt/Casts.h"
#include "adt/DefaultInitAllocatorAdaptor.h"
#include "common/ChecksumFile.h"
#include <chrono>
#include <cstdint>
#include <ctime>
#include <memory>
#include <ratio>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>
#include <benchmark/benchmark.h>

#ifdef HAVE_OPENMP
#include <omp.h>
#endif

#define HAVE_STEADY_CLOCK

using rawspeed::CameraMetaData;
using rawspeed::FileReader;
using rawspeed::RawImage;
using rawspeed::RawParser;

namespace {

int currThreadCount;

} // namespace

extern "C" int RAWSPEED_READONLY rawspeed_get_number_of_processor_cores() {
  return currThreadCount;
}

namespace {

struct CPUClock final {
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
struct ChooseSteadyClock final {
  using type = std::chrono::high_resolution_clock;
};

template <> struct ChooseSteadyClock<false> {
  using type = std::chrono::steady_clock;
};
#endif

struct ChooseClockType final {
#if defined(HAVE_STEADY_CLOCK)
  using type = ChooseSteadyClock<>::type;
#else
  using type = std::chrono::high_resolution_clock;
#endif
};

template <typename Clock, typename period = std::ratio<1, 1>>
struct Timer final {
  using rep = double;
  using duration = std::chrono::duration<rep, period>;

  mutable typename Clock::time_point start = Clock::now();

  duration operator()() const {
    duration elapsed = Clock::now() - start;
    start = Clock::now();
    return elapsed;
  }
};

// Lazy cache for the referenced file's content - not actually read until
// requested the first time.
struct Entry final {
  rawspeed::ChecksumFileEntry Name;
  std::unique_ptr<std::vector<
      uint8_t, rawspeed::DefaultInitAllocatorAdaptor<
                   uint8_t, rawspeed::AlignedAllocator<uint8_t, 16>>>>
      Storage;
  rawspeed::Buffer Content;

  const rawspeed::Buffer& getFileContents() {
    if (Storage)
      return Content;

    std::tie(Storage, Content) =
        FileReader(Name.FullFileName.c_str()).readFile();
    return Content;
  }
};

inline void BM_RawSpeed(benchmark::State& state, Entry* entry, int threads) {
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

    pixels = rawspeed::implicit_cast<unsigned>(raw->getUncroppedDim().area());
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

void addBench(Entry* entry, std::string tName, int threads) {
  tName += std::to_string(threads);

  auto* b = benchmark::RegisterBenchmark(tName, &BM_RawSpeed, entry, threads);
  b->Unit(benchmark::kMillisecond);
  b->UseRealTime();
  b->MeasureProcessCPUTime();
}

} // namespace

int main(int argc_, char** argv_) {
  benchmark::Initialize(&argc_, argv_);

  auto argv = rawspeed::Array1DRef(argv_, argc_);

  auto hasFlag = [argv](std::string_view flag) {
    int found = 0;
    for (int i = 1; i < argv.size(); ++i) {
      if (!argv(i) || argv(i) != flag)
        continue;
      found = i;
      argv(i) = nullptr;
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
  if (useChecksumFile && useChecksumFile + 1 < argv.size()) {
    char*& checksumFileRepo = argv(useChecksumFile + 1);
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
  for (int i = 1; i < argv.size(); i++) {
    if (!argv(i))
      continue;

    Entry Entry;
    const char* fName = argv(i);
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
