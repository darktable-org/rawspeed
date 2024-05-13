/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser

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
#include "adt/Array2DRef.h"
#include "adt/Casts.h"
#include "adt/DefaultInitAllocatorAdaptor.h"
#include "adt/NotARational.h"
#include "md5.h"
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

#if !defined(__has_feature) || !__has_feature(thread_sanitizer)
#include <iomanip>
#endif

using std::chrono::steady_clock;

using rawspeed::CameraMetaData;
using rawspeed::FileReader;
using rawspeed::getU16BE;
using rawspeed::getU32LE;
using rawspeed::iPoint2D;
using rawspeed::RawImage;
using rawspeed::RawParser;
using rawspeed::RawspeedException;
using rawspeed::roundUp;
using std::cerr;
using std::cout;
using std::ifstream;
using std::istreambuf_iterator;
using std::map;
using std::ofstream;
using std::ostringstream;
using std::vector;

#if !defined(__has_feature) || !__has_feature(thread_sanitizer)
using std::internal;
using std::left;
using std::setw;
#endif

namespace rawspeed::rstest {

std::string img_hash(const rawspeed::RawImage& r);

void writePPM(const rawspeed::RawImage& raw, const std::string& fn);
void writePFM(const rawspeed::RawImage& raw, const std::string& fn);

md5::MD5Hasher::state_type imgDataHash(const rawspeed::RawImage& raw);

void writeImage(const rawspeed::RawImage& raw, const std::string& fn);

struct options final {
  bool create;
  bool force;
  bool dump;
};

int64_t process(const std::string& filename,
                const rawspeed::CameraMetaData* metadata, const options& o);

class RstestHashMismatch final : public rawspeed::RawspeedException {
  void anchor() const override;

public:
  int64_t time;

  explicit RAWSPEED_UNLIKELY_FUNCTION RAWSPEED_NOINLINE
  RstestHashMismatch(const char* msg, int64_t time_)
      : RawspeedException(msg), time(time_) {}
};

void RstestHashMismatch::anchor() const {
  // Empty out-of-line definition for the purpose of anchoring
  // the class's vtable to this Translational Unit.
}

struct Timer final {
  mutable std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  int64_t operator()() const {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start)
                  .count();
    start = std::chrono::steady_clock::now();
    return ms;
  }
};

// yes, this is not cool. but i see no way to compute the hash of the
// full image, without duplicating image, and copying excluding padding
md5::MD5Hasher::state_type imgDataHash(const RawImage& raw) {
  const rawspeed::Array2DRef<std::byte> img =
      raw->getByteDataAsUncroppedArray2DRef();

  vector<md5::MD5Hasher::state_type> line_hashes(img.height());

  for (int j = 0; j < img.height(); j++) {
    line_hashes[j] = md5::md5_hash(reinterpret_cast<const uint8_t*>(&img(j, 0)),
                                   img.width());
  }

  auto ret = md5::md5_hash(reinterpret_cast<const uint8_t*>(&line_hashes[0]),
                           sizeof(line_hashes[0]) * line_hashes.size());

  return ret;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wframe-larger-than="
#pragma GCC diagnostic ignored "-Wstack-usage="

namespace {

void __attribute__((format(printf, 2, 3)))
APPEND(ostringstream* oss, const char* format, ...) {
  std::array<char, 1024> line;

  va_list args;
  va_start(args, format);
  vsnprintf(line.data(), sizeof(line), format, args);
  va_end(args);

  *oss << line.data();
}

} // namespace

std::string img_hash(const RawImage& r, bool noSamples) {
  ostringstream oss;

  if (noSamples)
    APPEND(&oss, "camera support status is unknown due to lack of samples\n");
  APPEND(&oss, "make: %s\n", r->metadata.make.c_str());
  APPEND(&oss, "model: %s\n", r->metadata.model.c_str());
  APPEND(&oss, "mode: %s\n", r->metadata.mode.c_str());

  APPEND(&oss, "canonical_make: %s\n", r->metadata.canonical_make.c_str());
  APPEND(&oss, "canonical_model: %s\n", r->metadata.canonical_model.c_str());
  APPEND(&oss, "canonical_alias: %s\n", r->metadata.canonical_alias.c_str());
  APPEND(&oss, "canonical_id: %s\n", r->metadata.canonical_id.c_str());

  APPEND(&oss, "isoSpeed: %d\n", r->metadata.isoSpeed);
  APPEND(&oss, "blackLevel: %d\n", r->blackLevel);

  APPEND(&oss, "whitePoint: ");
  if (!r->whitePoint)
    APPEND(&oss, "unknown");
  else
    APPEND(&oss, "%d", *r->whitePoint);
  APPEND(&oss, "\n");

  APPEND(&oss, "blackLevelSeparate: ");
  if (!r->blackLevelSeparate) {
    APPEND(&oss, "none");
  } else {
    APPEND(&oss, "(%i x %i)", r->blackLevelSeparate->width(),
           r->blackLevelSeparate->height());
    if (auto blackLevelSeparate1D = r->blackLevelSeparate->getAsArray1DRef();
        blackLevelSeparate1D && blackLevelSeparate1D->size() != 0) {
      for (auto l : *blackLevelSeparate1D)
        APPEND(&oss, " %d", l);
    }
  }
  APPEND(&oss, "\n");

  APPEND(&oss, "wbCoeffs: %f %f %f %f\n",
         implicit_cast<double>(r->metadata.wbCoeffs[0]),
         implicit_cast<double>(r->metadata.wbCoeffs[1]),
         implicit_cast<double>(r->metadata.wbCoeffs[2]),
         implicit_cast<double>(r->metadata.wbCoeffs[3]));

  APPEND(&oss, "colorMatrix:");
  if (r->metadata.colorMatrix.empty())
    APPEND(&oss, " (none)");
  else {
    for (const NotARational<int>& e : r->metadata.colorMatrix)
      APPEND(&oss, " %i/%i", e.num, e.den);
  }
  APPEND(&oss, "\n");

  APPEND(&oss, "isCFA: %d\n", r->isCFA);
  APPEND(&oss, "cfa: %s\n", r->cfa.asString().c_str());
  APPEND(&oss, "filters: 0x%x\n", r->cfa.getDcrawFilter());
  APPEND(&oss, "bpp: %d\n", r->getBpp());
  APPEND(&oss, "cpp: %d\n", r->getCpp());
  APPEND(&oss, "dataType: %u\n", static_cast<unsigned>(r->getDataType()));

  const iPoint2D dimUncropped = r->getUncroppedDim();
  APPEND(&oss, "dimUncropped: %dx%d\n", dimUncropped.x, dimUncropped.y);
  APPEND(&oss, "dimCropped: %dx%d\n", r->dim.x, r->dim.y);
  const iPoint2D cropTL = r->getCropOffset();
  APPEND(&oss, "cropOffset: %dx%d\n", cropTL.x, cropTL.y);

  // NOTE: pitch is internal property, a function of dimUncropped.x, bpp and
  // some additional padding overhead, to align each line length to be a
  // multiple of (currently) 16 bytes. And maybe with some additional
  // const offset. there is no point in showing it here, it may differ.
  // APPEND(&oss, "pitch: %d\n", r->pitch);

  APPEND(&oss, "blackAreas: ");
  for (auto ba : r->blackAreas)
    APPEND(&oss, "%d:%dx%d, ", ba.isVertical, ba.offset, ba.size);
  APPEND(&oss, "\n");

  APPEND(&oss, "fuji_rotation_pos: %d\n", r->metadata.fujiRotationPos);
  APPEND(&oss, "pixel_aspect_ratio: %f\n", r->metadata.pixelAspectRatio);

  APPEND(&oss, "badPixelPositions: ");
  {
    MutexLocker guard(&r->mBadPixelMutex);
    for (uint32_t p : r->mBadPixelPositions)
      APPEND(&oss, "%d, ", p);
  }

  APPEND(&oss, "\n");

  rawspeed::md5::MD5Hasher::state_type hash_of_line_hashes = imgDataHash(r);
  APPEND(&oss, "md5sum of per-line md5sums: %s\n",
         rawspeed::md5::hash_to_string(hash_of_line_hashes).c_str());

  for (const auto errors = r->getErrors(); const std::string& e : errors)
    APPEND(&oss, "WARNING: [rawspeed] %s\n", e.c_str());

  return oss.str();
}

auto fclose = [](std::FILE* fp) { std::fclose(fp); };
using file_ptr = std::unique_ptr<FILE, decltype(fclose)>;

void writePPM(const RawImage& raw, const std::string& fn) {
  file_ptr f(fopen((fn + ".ppm").c_str(), "wb"), fclose);

  const iPoint2D dimUncropped = raw->getUncroppedDim();
  int width = dimUncropped.x;
  int height = dimUncropped.y;
  std::string format = raw->getCpp() == 1 ? "P5" : "P6";

  // Write PPM header
  fprintf(f.get(), "%s\n%d %d\n65535\n", format.c_str(), width, height);

  width *= raw->getCpp();

  // Write pixels
  const Array2DRef<uint16_t> img = raw->getU16DataAsUncroppedArray2DRef();
  for (int y = 0; y < height; ++y) {
    // PPM is big-endian
    for (int x = 0; x < width; ++x)
      img(y, x) = getU16BE(&img(y, x));

    fwrite(&img(y, 0), sizeof(decltype(img)::value_type), width, f.get());
  }
}

void writePFM(const RawImage& raw, const std::string& fn) {
  file_ptr f(fopen((fn + ".pfm").c_str(), "wb"), fclose);

  const iPoint2D dimUncropped = raw->getUncroppedDim();
  int width = dimUncropped.x;
  int height = dimUncropped.y;
  std::string format = raw->getCpp() == 1 ? "Pf" : "PF";

  // Write PFM header. if scale < 0, it is little-endian, if >= 0 - big-endian
  int len = fprintf(f.get(), "%s\n%d %d\n-1.0", format.c_str(), width, height);

  // make sure that data starts at aligned offset. for sse
  static const auto dataAlignment = 16;

  // regardless of padding, we need to write \n separator
  const int realLen = len + 1;
  // the first byte after that \n will be aligned
  const auto paddedLen =
      rawspeed::implicit_cast<int>(roundUp(realLen, dataAlignment));
  assert(paddedLen > len);
  assert(rawspeed::isAligned(paddedLen, dataAlignment));

  // how much padding?
  const int padding = paddedLen - realLen;
  assert(padding >= 0);
  assert(rawspeed::isAligned(realLen + padding, dataAlignment));

  // and actually write padding + new line
  len += fprintf(f.get(), "%0*i\n", padding, 0);
  assert(paddedLen == len);

  // did we write a multiple of an alignment value?
  assert(rawspeed::isAligned(len, dataAlignment));
  assert(ftell(f.get()) == len);
  assert(rawspeed::isAligned(ftell(f.get()), dataAlignment));

  width *= raw->getCpp();

  // Write pixels
  const Array2DRef<float> img = raw->getF32DataAsUncroppedArray2DRef();
  for (int y = 0; y < height; ++y) {
    // NOTE: pfm has rows in reverse order
    const int row_in = height - 1 - y;

    // PFM can have any endianness, let's write little-endian
    for (int x = 0; x < width; ++x)
      img(row_in, x) = std::bit_cast<float>(getU32LE(&img(row_in, x)));

    fwrite(&img(row_in, 0), sizeof(decltype(img)::value_type), width, f.get());
  }
}

void writeImage(const RawImage& raw, const std::string& fn) {
  switch (raw->getDataType()) {
  case RawImageType::UINT16:
    writePPM(raw, fn);
    return;
  case RawImageType::F32:
    writePFM(raw, fn);
    return;
  }
  __builtin_unreachable();
}

int64_t process(const std::string& filename, const CameraMetaData* metadata,
                const options& o) {

  const std::string hashfile(filename + ".hash");

  // if creating hash and hash exists -> skip current file
  // if not creating and hash is missing -> skip as well
  // unless in force mode
  ifstream hf(hashfile);
  if (hf.good() == o.create && !o.force) {
#if !defined(__has_feature) || !__has_feature(thread_sanitizer)
#ifdef HAVE_OPENMP
#pragma omp critical(io)
#endif
    cout << left << setw(55) << filename << ": hash "
         << (o.create ? "exists" : "missing") << ", skipping" << '\n';
#endif
    return 0;
  }

// to narrow down the list of files that could have causes the crash
#if !defined(__has_feature) || !__has_feature(thread_sanitizer)
#ifdef HAVE_OPENMP
#pragma omp critical(io)
#endif
  cout << left << setw(55) << filename << ": starting decoding ... " << '\n';
#endif

  FileReader reader(filename.c_str());

  std::unique_ptr<std::vector<
      uint8_t, rawspeed::DefaultInitAllocatorAdaptor<
                   uint8_t, rawspeed::AlignedAllocator<uint8_t, 16>>>>
      storage;
  rawspeed::Buffer buf;
  std::tie(storage, buf) = reader.readFile();

  Timer t;

  RawParser parser(buf);
  auto decoder(parser.getDecoder(metadata));
  // RawDecoder* decoder = parseRaw( map );

  decoder->failOnUnknown = false;
  decoder->checkSupport(metadata);
  bool noSamples = decoder->noSamples;

  decoder->decodeRaw();
  decoder->decodeMetaData(metadata);
  RawImage raw = decoder->mRaw;
  // RawImage raw = decoder->decode();

  auto time = t();
#if !defined(__has_feature) || !__has_feature(thread_sanitizer)
#ifdef HAVE_OPENMP
#pragma omp critical(io)
#endif
  cout << left << setw(55) << filename << ": " << internal << setw(3)
       << buf.getSize() / 1000000 << " MB / " << setw(4) << time << " ms"
       << '\n';
#endif

  if (o.create) {
    // write the hash. if force is set, then we are potentially overwriting here
    ofstream f(hashfile);
    f << img_hash(raw, noSamples);
    if (o.dump)
      writeImage(raw, filename);
  } else {
    // do generate the hash string regardless.
    std::string h = img_hash(raw, noSamples);

    // normally, here we would compare the old hash with the new one
    // but if the force is set, and the hash does not exist, do nothing.
    if (!hf.good() && o.force)
      return time;

    std::string truth((istreambuf_iterator<char>(hf)),
                      istreambuf_iterator<char>());
    if (h != truth) {
      ofstream f(filename + ".hash.failed");
      f << h;
      if (o.dump)
        writeImage(raw, filename + ".failed");
      throw RstestHashMismatch("hash/metadata mismatch", time);
    }
  }

  return time;
}

#pragma GCC diagnostic pop

namespace {

int results(const map<std::string, std::string, std::less<>>& failedTests,
            const options& o) {
  if (failedTests.empty()) {
    cout << "All good, ";
    if (!o.create)
      cout << "no tests failed!" << '\n';
    else
      cout << "all hashes created!" << '\n';
    return 0;
  }

  cerr << "WARNING: the following " << failedTests.size()
       << " tests have failed:\n";

  bool rstestlog = false;
  for (const auto& [test, msg] : failedTests) {
    cerr << msg << "\n";
#ifndef WIN32
    const std::string oldhash(test + ".hash");
    const std::string newhash(oldhash + ".failed");

    // if neither hashes exist, nothing to append...
    if (!(ifstream(oldhash).good() || ifstream(newhash).good()))
      continue;

    rstestlog = true;

    // DIFF(1): -N, --new-file  treat absent files as empty
    std::string cmd(R"(diff -N -u0 ")");
    cmd += oldhash;
    cmd += R"(" ")";
    cmd += newhash;
    cmd += R"(" >> rstest.log)";
    // NOLINTNEXTLINE(concurrency-mt-unsafe): we are a single thread here only.
    if (system(cmd.c_str())) {
    }
  }
#endif

  if (rstestlog)
    cerr << "See rstest.log for details.\n";

  return 1;
}

int usage(const char* progname) {
  cout << "usage: " << progname << R"(
  [-h] print this help
  [-c] for each file: decode, compute hash and store it.
       If hash exists, it does not recompute it, unless option -f is set!
  [-f] if -c is set, then it will final the existing hashes.
       If -c is not set, and the hash does not exist, then just decode,
       but do not write the hash!
  [-d] store decoded image as PPM
  <FILE[S]> the file[s] to work on.

  With no options given, each raw with an accompanying hash will be decoded
  and compared (unless option -f is set!) to the existing hash. A summary of
  all errors/failed hash comparisons will be reported at the end.

  Suggested workflow for easy regression testing:
    1. remove all .hash files and build 'trusted' version of this program
    2. run with option '-c' -> creates .hash for all supported files
    3. build new version to test for regressions
    4. run with no option   -> checks files with existing .hash
  If the second run shows no errors, you have no regressions,
  otherwise, the diff between hashes is appended to rstest.log
)";
  return 0;
}

} // namespace

} // namespace rawspeed::rstest

using rawspeed::rstest::options;
using rawspeed::rstest::process;
using rawspeed::rstest::results;
using rawspeed::rstest::usage;

int main(int argc_, char** argv_) {
  auto argv = rawspeed::Array1DRef(argv_, argc_);

  int remaining_argc = argv.size();

  auto hasFlag = [&remaining_argc, argv](std::string_view flag) {
    bool found = false;
    for (int i = 1; i < argv.size(); ++i) {
      if (!argv(i) || argv(i) != flag)
        continue;
      found = true;
      argv(i) = nullptr;
      remaining_argc--;
    }
    return found;
  };

  if (1 == argv.size() || hasFlag("-h"))
    return usage(argv(0));

  options o;
  o.create = hasFlag("-c");
  o.force = hasFlag("-f");
  o.dump = hasFlag("-d");

#ifdef HAVE_PUGIXML
  const CameraMetaData metadata(RAWSPEED_SOURCE_DIR "/data/cameras.xml");
#else
  const CameraMetaData metadata{};
#endif

  int64_t time = 0;
  map<std::string, std::string, std::less<>> failedTests;
#ifdef HAVE_OPENMP
#pragma omp parallel for default(none) firstprivate(argv, o) shared(metadata)  \
    shared(cerr, failedTests) schedule(dynamic, 1)                             \
    reduction(+ : time) if (remaining_argc > 2)
#endif
  for (int i = 1; i < argv.size(); ++i) {
    if (!argv(i))
      continue;

    try {
      try {
        time += process(argv(i), &metadata, o);
      } catch (const rawspeed::rstest::RstestHashMismatch& e) {
        time += e.time;
        throw;
      }
    } catch (const RawspeedException& e) {
#ifdef HAVE_OPENMP
#pragma omp critical(io)
#endif
      {
        std::string msg = std::string(argv(i)) + " failed: " + e.what();
#if !defined(__has_feature) || !__has_feature(thread_sanitizer)
        cerr << msg << '\n';
#endif
        failedTests.try_emplace(argv(i), msg);
      }
    } catch (...) {
      // We should not get any other exception type here.
      __builtin_unreachable();
    }
  }

  cout << "Total decoding time: "
       << rawspeed::implicit_cast<double>(time) / 1000.0 << "s" << '\n'
       << '\n';

  return results(failedTests, o);
}
