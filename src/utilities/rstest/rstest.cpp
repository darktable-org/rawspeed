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

#include "md5.h"     // for md5_state, md5_hash, hash_to_string, md5_init
#include <algorithm> // for fill, max
#include <array>     // for array
#include <cassert>   // for assert
#include <chrono>    // for milliseconds, steady_clock, duration_cast
#include <cstdarg>   // for va_end, va_list, va_start
#include <cstdint>   // for uint16_t, uint32_t, uint8_t
#include <cstdio>    // for fprintf, fclose, fopen, ftell, fwrite, size_t
#include <cstdlib>   // for system
#include <fstream>   // IWYU pragma: keep
#include <iostream>  // for cout, cerr
#include <iterator>  // for istreambuf_iterator, operator!=
#include <map>       // for map
#include <memory>    // for allocator, unique_ptr
#include <sstream>   // IWYU pragma: keep
#include <string>    // for string, operator+, operator<<, char_traits
#include <utility>   // for pair
#include <vector>    // for vector
// IWYU pragma: no_include <ext/alloc_traits.h>

#if !defined(__has_feature) || !__has_feature(thread_sanitizer)
#include <iomanip> // for operator<<, setw
#endif

using std::chrono::steady_clock;
using std::string;
using std::ostringstream;
using std::vector;
using std::ifstream;
using std::istreambuf_iterator;
using std::ofstream;
using std::cout;
using std::endl;
using std::map;
using std::cerr;
using rawspeed::CameraMetaData;
using rawspeed::FileReader;
using rawspeed::RawParser;
using rawspeed::RawImage;
using rawspeed::iPoint2D;
using rawspeed::TYPE_USHORT16;
using rawspeed::TYPE_FLOAT32;
using rawspeed::getU16BE;
using rawspeed::getU32LE;
using rawspeed::roundUp;
using rawspeed::RawspeedException;

#if !defined(__has_feature) || !__has_feature(thread_sanitizer)
using std::setw;
using std::left;
using std::internal;
#endif

namespace rawspeed::rstest {

std::string img_hash(const rawspeed::RawImage& r);

void writePPM(const rawspeed::RawImage& raw, const std::string& fn);
void writePFM(const rawspeed::RawImage& raw, const std::string& fn);

md5::md5_state imgDataHash(const rawspeed::RawImage& raw);

void writeImage(const rawspeed::RawImage& raw, const std::string& fn);

struct options {
  bool create;
  bool force;
  bool dump;
};

size_t process(const std::string& filename,
               const rawspeed::CameraMetaData* metadata, const options& o);

class RstestHashMismatch final : public rawspeed::RawspeedException {
public:
  size_t time;

  explicit RAWSPEED_UNLIKELY_FUNCTION RAWSPEED_NOINLINE
  RstestHashMismatch(const char* msg, size_t time_)
      : RawspeedException(msg), time(time_) {}
};

struct Timer {
  mutable std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  size_t operator()() const {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start)
                  .count();
    start = std::chrono::steady_clock::now();
    return ms;
  }
};

// yes, this is not cool. but i see no way to compute the hash of the
// full image, without duplicating image, and copying excluding padding
md5::md5_state imgDataHash(const RawImage& raw) {
  md5::md5_state ret = md5::md5_init;

  const iPoint2D dimUncropped = raw->getUncroppedDim();

  vector<md5::md5_state> line_hashes;
  line_hashes.resize(dimUncropped.y, md5::md5_init);

  for (int j = 0; j < dimUncropped.y; j++) {
    auto* d = raw->getDataUncropped(0, j);
    md5::md5_hash(d, raw->pitch - raw->padding, &line_hashes[j]);
  }

  md5::md5_hash(reinterpret_cast<const uint8_t*>(&line_hashes[0]),
                sizeof(line_hashes[0]) * line_hashes.size(), &ret);

  return ret;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wframe-larger-than="
#pragma GCC diagnostic ignored "-Wstack-usage="

static void __attribute__((format(printf, 2, 3)))
APPEND(ostringstream* oss, const char* format, ...) {
  std::array<char, 1024> line;

  va_list args;
  va_start(args, format);
  vsnprintf(line.data(), sizeof(line), format, args);
  va_end(args);

  *oss << line.data();
}

string img_hash(const RawImage& r) {
  ostringstream oss;

  APPEND(&oss, "make: %s\n", r->metadata.make.c_str());
  APPEND(&oss, "model: %s\n", r->metadata.model.c_str());
  APPEND(&oss, "mode: %s\n", r->metadata.mode.c_str());

  APPEND(&oss, "canonical_make: %s\n", r->metadata.canonical_make.c_str());
  APPEND(&oss, "canonical_model: %s\n", r->metadata.canonical_model.c_str());
  APPEND(&oss, "canonical_alias: %s\n", r->metadata.canonical_alias.c_str());
  APPEND(&oss, "canonical_id: %s\n", r->metadata.canonical_id.c_str());

  APPEND(&oss, "isoSpeed: %d\n", r->metadata.isoSpeed);
  APPEND(&oss, "blackLevel: %d\n", r->blackLevel);
  APPEND(&oss, "whitePoint: %d\n", r->whitePoint);

  APPEND(&oss, "blackLevelSeparate: %d %d %d %d\n", r->blackLevelSeparate[0],
         r->blackLevelSeparate[1], r->blackLevelSeparate[2],
         r->blackLevelSeparate[3]);

  APPEND(&oss, "wbCoeffs: %f %f %f %f\n", r->metadata.wbCoeffs[0],
         r->metadata.wbCoeffs[1], r->metadata.wbCoeffs[2],
         r->metadata.wbCoeffs[3]);

  APPEND(&oss, "isCFA: %d\n", r->isCFA);
  APPEND(&oss, "cfa: %s\n", r->cfa.asString().c_str());
  APPEND(&oss, "filters: 0x%x\n", r->cfa.getDcrawFilter());
  APPEND(&oss, "bpp: %d\n", r->getBpp());
  APPEND(&oss, "cpp: %d\n", r->getCpp());
  APPEND(&oss, "dataType: %d\n", r->getDataType());

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

  rawspeed::md5::md5_state hash_of_line_hashes = imgDataHash(r);
  APPEND(&oss, "md5sum of per-line md5sums: %s\n",
         rawspeed::md5::hash_to_string(hash_of_line_hashes).c_str());

  const auto errors = r->getErrors();
  for (const string& e : errors)
    APPEND(&oss, "WARNING: [rawspeed] %s\n", e.c_str());

#undef APPEND

  return oss.str();
}

using file_ptr = std::unique_ptr<FILE, decltype(&fclose)>;

void writePPM(const RawImage& raw, const string& fn) {
  file_ptr f(fopen((fn + ".ppm").c_str(), "wb"), &fclose);

  const iPoint2D dimUncropped = raw->getUncroppedDim();
  int width = dimUncropped.x;
  int height = dimUncropped.y;
  string format = raw->getCpp() == 1 ? "P5" : "P6";

  // Write PPM header
  fprintf(f.get(), "%s\n%d %d\n65535\n", format.c_str(), width, height);

  width *= raw->getCpp();

  // Write pixels
  for (int y = 0; y < height; ++y) {
    auto* row = reinterpret_cast<uint16_t*>(raw->getDataUncropped(0, y));
    // PPM is big-endian
    for (int x = 0; x < width; ++x)
      row[x] = getU16BE(row + x);

    fwrite(row, sizeof(*row), width, f.get());
  }
}

void writePFM(const RawImage& raw, const string& fn) {
  file_ptr f(fopen((fn + ".pfm").c_str(), "wb"), &fclose);

  const iPoint2D dimUncropped = raw->getUncroppedDim();
  int width = dimUncropped.x;
  int height = dimUncropped.y;
  string format = raw->getCpp() == 1 ? "Pf" : "PF";

  // Write PFM header. if scale < 0, it is little-endian, if >= 0 - big-endian
  int len = fprintf(f.get(), "%s\n%d %d\n-1.0", format.c_str(), width, height);

  // make sure that data starts at aligned offset. for sse
  static const auto dataAlignment = 16;

  // regardless of padding, we need to write \n separator
  const int realLen = len + 1;
  // the first byte after that \n will be aligned
  const int paddedLen = roundUp(realLen, dataAlignment);
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
  for (int y = 0; y < height; ++y) {
    // NOTE: pfm has rows in reverse order
    const int row_in = height - 1 - y;
    auto* row = reinterpret_cast<float*>(raw->getDataUncropped(0, row_in));

    // PFM can have any endiannes, let's write little-endian
    for (int x = 0; x < width; ++x)
      row[x] = getU32LE(row + x);

    fwrite(row, sizeof(*row), width, f.get());
  }
}

void writeImage(const RawImage& raw, const string& fn) {
  switch (raw->getDataType()) {
  case TYPE_USHORT16:
    writePPM(raw, fn);
    break;
  case TYPE_FLOAT32:
    writePFM(raw, fn);
    break;
  default:
    __builtin_unreachable();
  }
}

size_t process(const string& filename, const CameraMetaData* metadata,
               const options& o) {

  const string hashfile(filename + ".hash");

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
         << (o.create ? "exists" : "missing") << ", skipping" << endl;
#endif
    return 0;
  }

// to narrow down the list of files that could have causes the crash
#if !defined(__has_feature) || !__has_feature(thread_sanitizer)
#ifdef HAVE_OPENMP
#pragma omp critical(io)
#endif
  cout << left << setw(55) << filename << ": starting decoding ... " << endl;
#endif

  FileReader reader(filename.c_str());

  auto map(reader.readFile());
  // Buffer* map = readFile( argv[1] );

  Timer t;

  RawParser parser(map.get());
  auto decoder(parser.getDecoder(metadata));
  // RawDecoder* decoder = parseRaw( map );

  decoder->failOnUnknown = false;
  decoder->checkSupport(metadata);

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
       << map->getSize() / 1000000 << " MB / " << setw(4) << time << " ms"
       << endl;
#endif

  if (o.create) {
    // write the hash. if force is set, then we are potentially overwriting here
    ofstream f(hashfile);
    f << img_hash(raw);
    if (o.dump)
      writeImage(raw, filename);
  } else {
    // do generate the hash string regardless.
    string h = img_hash(raw);

    // normally, here we would compare the old hash with the new one
    // but if the force is set, and the hash does not exist, do nothing.
    if (!hf.good() && o.force)
      return time;

    string truth((istreambuf_iterator<char>(hf)), istreambuf_iterator<char>());
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

static int results(const map<string, string>& failedTests, const options& o) {
  if (failedTests.empty()) {
    cout << "All good, ";
    if (!o.create)
      cout << "no tests failed!" << endl;
    else
      cout << "all hashes created!" << endl;
    return 0;
  }

  cerr << "WARNING: the following " << failedTests.size()
       << " tests have failed:\n";

  bool rstestlog = false;
  for (const auto& i : failedTests) {
    cerr << i.second << "\n";
#ifndef WIN32
    const string oldhash(i.first + ".hash");
    const string newhash(oldhash + ".failed");

    ifstream oldfile(oldhash);
    ifstream newfile(newhash);

    // if neither hashes exist, nothing to append...
    if (!(oldfile.good() || newfile.good()))
      continue;

    rstestlog = true;

    // DIFF(1): -N, --new-file  treat absent files as empty
    string cmd(R"(diff -N -u0 ")");
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

static int usage(const char* progname) {
  cout << "usage: " << progname << R"(
  [-h] print this help
  [-c] for each file: decode, compute hash and store it.
       If hash exists, it does not recompute it, unless option -f is set!
  [-f] if -c is set, then it will override the existing hashes.
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

} // namespace rawspeed::rstest

using rawspeed::rstest::usage;
using rawspeed::rstest::options;
using rawspeed::rstest::process;
using rawspeed::rstest::results;

int main(int argc, char **argv) {
  int remaining_argc = argc;

  auto hasFlag = [argc, &remaining_argc, argv](const string& flag) {
    bool found = false;
    for (int i = 1; i < argc; ++i) {
      if (!argv[i] || argv[i] != flag)
        continue;
      found = true;
      argv[i] = nullptr;
      remaining_argc--;
    }
    return found;
  };

  if (1 == argc || hasFlag("-h"))
    return usage(argv[0]);

  options o;
  o.create = hasFlag("-c");
  o.force = hasFlag("-f");
  o.dump = hasFlag("-d");

#ifdef HAVE_PUGIXML
  const CameraMetaData metadata(RAWSPEED_SOURCE_DIR "/data/cameras.xml");
#else
  const CameraMetaData metadata{};
#endif

  size_t time = 0;
  map<string, string> failedTests;
#ifdef HAVE_OPENMP
#pragma omp parallel for default(none) firstprivate(argc, argv, o) \
    OMPSHAREDCLAUSE(metadata) shared(cerr, failedTests) schedule(dynamic, 1) \
    reduction(+ : time) if(remaining_argc > 2)
#endif
  for (int i = 1; i < argc; ++i) {
    if (!argv[i])
      continue;

    try {
      try {
        time += process(argv[i], &metadata, o);
      } catch (rawspeed::rstest::RstestHashMismatch& e) {
        time += e.time;
        throw;
      }
    } catch (RawspeedException& e) {
#ifdef HAVE_OPENMP
#pragma omp critical(io)
#endif
      {
        string msg = string(argv[i]) + " failed: " + e.what();
#if !defined(__has_feature) || !__has_feature(thread_sanitizer)
        cerr << msg << endl;
#endif
        failedTests.emplace(argv[i], msg);
      }
    }
  }

  cout << "Total decoding time: " << time / 1000.0 << "s" << endl << endl;

  return results(failedTests, o);
}
