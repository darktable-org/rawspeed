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

#include "md5.h"           // for md5_hash
#include <cassert>         // for assert
#include <chrono>          // for milliseconds, steady_clock, duration, dur...
#include <cstdint>         // for uint8_t
#include <cstdio>          // for snprintf, size_t, fclose, fopen, fprintf
#include <cstdlib>         // for system
#include <fstream>         // IWYU pragma: keep
#include <iomanip>         // for operator<<, setw
#include <iostream>        // for cout, cerr, left, internal
#include <map>             // for map
#include <memory>          // for unique_ptr, allocator
#include <sstream>         // IWYU pragma: keep
#include <string>          // for string, char_traits, operator+, operator<<
#include <type_traits>     // for enable_if<>::type
#include <utility>         // for pair
#include <vector>          // for vector
// IWYU pragma: no_include <ext/alloc_traits.h>

#ifdef _OPENMP
#include <omp.h>
#endif

// define this function, it is only declared in rawspeed:
#ifdef _OPENMP
int rawspeed_get_number_of_processor_cores() { return omp_get_num_procs(); }
#else
int __attribute__((const)) rawspeed_get_number_of_processor_cores() {
  return 1;
}
#endif

using std::chrono::steady_clock;
using std::string;
using std::ostringstream;
using std::vector;
using std::ifstream;
using std::istreambuf_iterator;
using std::ofstream;
using std::cout;
using std::setw;
using std::left;
using std::endl;
using std::internal;
using std::map;
using std::cerr;
using rawspeed::CameraMetaData;
using rawspeed::FileReader;
using rawspeed::RawParser;
using rawspeed::RawImage;
using rawspeed::uchar8;
using rawspeed::uint32;
using rawspeed::iPoint2D;
using rawspeed::TYPE_USHORT16;
using rawspeed::TYPE_FLOAT32;
using rawspeed::getU16BE;
using rawspeed::getU32LE;
using rawspeed::isAligned;
using rawspeed::roundUp;
using rawspeed::RawspeedException;

namespace rawspeed {

namespace rstest {

std::string img_hash(rawspeed::RawImage& r);

void writePPM(const rawspeed::RawImage& raw, const std::string& fn);
void writePFM(const rawspeed::RawImage& raw, const std::string& fn);

md5::md5_state imgDataHash(const rawspeed::RawImage& raw);

void writeImage(const rawspeed::RawImage& raw, const std::string& fn);

size_t process(const std::string& filename,
               const rawspeed::CameraMetaData* metadata, bool create,
               bool dump);

class RstestHashMismatch final : public rawspeed::RawspeedException {
public:
  explicit RstestHashMismatch(const std::string& msg)
      : RawspeedException(msg) {}
  explicit RstestHashMismatch(const char* msg) : RawspeedException(msg) {}
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
    md5::md5_hash(d, raw->pitch - raw->padding, line_hashes[j]);
  }

  md5::md5_hash(reinterpret_cast<const uint8_t*>(&line_hashes[0]),
                sizeof(line_hashes[0]) * line_hashes.size(), ret);

  return ret;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wframe-larger-than="
#pragma GCC diagnostic ignored "-Wstack-usage="

string img_hash(RawImage &r) {
  ostringstream oss;
  char line[1024];

#define APPEND(...)                                                            \
  do {                                                                         \
    snprintf(line, sizeof(line), __VA_ARGS__);                                 \
    oss << line;                                                               \
  } while (false)

  APPEND("make: %s\n", r->metadata.make.c_str());
  APPEND("model: %s\n", r->metadata.model.c_str());
  APPEND("mode: %s\n", r->metadata.mode.c_str());

  APPEND("canonical_make: %s\n", r->metadata.canonical_make.c_str());
  APPEND("canonical_model: %s\n", r->metadata.canonical_model.c_str());
  APPEND("canonical_alias: %s\n", r->metadata.canonical_alias.c_str());
  APPEND("canonical_id: %s\n", r->metadata.canonical_id.c_str());

  APPEND("isoSpeed: %d\n", r->metadata.isoSpeed);
  APPEND("blackLevel: %d\n", r->blackLevel);
  APPEND("whitePoint: %d\n", r->whitePoint);

  APPEND("blackLevelSeparate: %d %d %d %d\n", r->blackLevelSeparate[0],
         r->blackLevelSeparate[1], r->blackLevelSeparate[2],
         r->blackLevelSeparate[3]);

  APPEND("wbCoeffs: %f %f %f %f\n", r->metadata.wbCoeffs[0],
         r->metadata.wbCoeffs[1], r->metadata.wbCoeffs[2],
         r->metadata.wbCoeffs[3]);

  APPEND("isCFA: %d\n", r->isCFA);
  APPEND("cfa: %s\n", r->cfa.asString().c_str());
  APPEND("filters: 0x%x\n", r->cfa.getDcrawFilter());
  APPEND("bpp: %d\n", r->getBpp());
  APPEND("cpp: %d\n", r->getCpp());
  APPEND("dataType: %d\n", r->getDataType());

  const iPoint2D dimUncropped = r->getUncroppedDim();
  APPEND("dimUncropped: %dx%d\n", dimUncropped.x, dimUncropped.y);
  APPEND("dimCropped: %dx%d\n", r->dim.x, r->dim.y);
  const iPoint2D cropTL = r->getCropOffset();
  APPEND("cropOffset: %dx%d\n", cropTL.x, cropTL.y);

  // NOTE: pitch is internal property, a function of dimUncropped.x, bpp and
  // some additional padding overhead, to align each line lenght to be a
  // multiple of (currently) 16 bytes. And maybe with some additional
  // const offset. there is no point in showing it here, it may differ.
  // APPEND("pitch: %d\n", r->pitch);

  APPEND("blackAreas: ");
  for (auto ba : r->blackAreas)
    APPEND("%d:%dx%d, ", ba.isVertical, ba.offset, ba.size);
  APPEND("\n");

  APPEND("fuji_rotation_pos: %d\n", r->metadata.fujiRotationPos);
  APPEND("pixel_aspect_ratio: %f\n", r->metadata.pixelAspectRatio);

  APPEND("badPixelPositions: ");
  for (uint32 p : r->mBadPixelPositions)
    APPEND("%d, ", p);

  APPEND("\n");

  rawspeed::md5::md5_state hash_of_line_hashes = imgDataHash(r);
  APPEND("md5sum of per-line md5sums: %s\n",
         rawspeed::md5::hash_to_string(hash_of_line_hashes).c_str());

  for (const string& e : r->errors)
    APPEND("WARNING: [rawspeed] %s\n", e.c_str());

#undef APPEND

  return oss.str();
}

void writePPM(const RawImage& raw, const string& fn) {
  FILE* f = fopen((fn + ".ppm").c_str(), "wb");

  int width = raw->dim.x;
  int height = raw->dim.y;
  string format = raw->getCpp() == 1 ? "P5" : "P6";

  // Write PPM header
  fprintf(f, "%s\n%d %d\n65535\n", format.c_str(), width, height);

  width *= raw->getCpp();

  // Write pixels
  for (int y = 0; y < height; ++y) {
    auto* row = reinterpret_cast<unsigned short*>(raw->getData(0, y));
    // PPM is big-endian
    for (int x = 0; x < width; ++x)
      row[x] = getU16BE(row + x);

    fwrite(row, sizeof(*row), width, f);
  }
  fclose(f);
}

void writePFM(const RawImage& raw, const string& fn) {
  FILE* f = fopen((fn + ".pfm").c_str(), "wb");

  int width = raw->dim.x;
  int height = raw->dim.y;
  string format = raw->getCpp() == 1 ? "Pf" : "PF";

  // Write PFM header. if scale < 0, it is little-endian, if >= 0 - big-endian
  int len = fprintf(f, "%s\n%d %d\n-1.0", format.c_str(), width, height);

  // make sure that data starts at aligned offset. for sse
  static const auto dataAlignment = 16;
  const int sseLen = roundUp(len, dataAlignment);
  len += fprintf(f, "%0*i\n", sseLen - len - 1, 0);

  // did we write a multiple of an alignment value?
  assert(isAligned(len, dataAlignment));
  assert(ftell(f) == len);
  assert(isAligned(ftell(f), dataAlignment));

  width *= raw->getCpp();

  // Write pixels
  for (int y = 0; y < height; ++y) {
    // NOTE: pfm has rows in reverse order
    const int row_in = height - 1 - y;
    auto* row = reinterpret_cast<float*>(raw->getData(0, row_in));

    // PFM can have any endiannes, let's write little-endian
    for (int x = 0; x < width; ++x)
      row[x] = getU32LE(row + x);

    fwrite(row, sizeof(*row), width, f);
  }
  fclose(f);
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
    break;
  }
}

size_t process(const string& filename, const CameraMetaData* metadata,
               bool create, bool dump) {

  const string hashfile(filename + ".hash");

  // if creating hash and hash exists -> skip current file
  // if not creating and hash is missing -> skip as well
  ifstream hf(hashfile);
  if (hf.good() == create) {
#ifdef _OPENMP
#pragma omp critical(io)
#endif
    cout << left << setw(55) << filename << ": hash "
         << (create ? "exists" : "missing") << ", skipping" << endl;
    return 0;
  }

// to narrow down the list of files that could have causes the crash
#ifdef _OPENMP
#pragma omp critical(io)
#endif
  cout << left << setw(55) << filename << ": starting decoding ... " << endl;

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
#ifdef _OPENMP
#pragma omp critical(io)
#endif
  cout << left << setw(55) << filename << ": " << internal << setw(3)
       << map->getSize() / 1000000 << " MB / " << setw(4) << time << " ms"
       << endl;

  if (create) {
    ofstream f(hashfile);
    f << img_hash(raw);
    if (dump)
      writeImage(raw, filename);
  } else {
    string truth((istreambuf_iterator<char>(hf)), istreambuf_iterator<char>());
    string h = img_hash(raw);
    if (h != truth) {
      ofstream f(filename + ".hash.failed");
      f << h;
      if (dump)
        writeImage(raw, filename + ".failed");
      throw RstestHashMismatch("hash/metadata mismatch");
    }
  }

  return time;
}

#pragma GCC diagnostic pop

static int results(const map<string, string>& failedTests) {
  if (failedTests.empty()) {
    cout << "All good, no tests failed!" << endl;
    return 0;
  }

  cerr << "WARNING: the following " << failedTests.size()
       << " tests have failed:\n";

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

    // DIFF(1): -N, --new-file  treat absent files as empty
    string cmd(R"(diff -N -u0 ")");
    cmd += oldhash;
    cmd += R"(" ")";
    cmd += newhash;
    cmd += R"(" >> rstest.log)";
    if (system(cmd.c_str())) {
    }
  }
#endif

  cerr << "See rstest.log for details.\n";

  return 1;
}

static int usage(const char* progname) {
  cout << "usage: " << progname << R"(
  [-h] print this help
  [-c] for each file: decode, compute hash and store it.
       If hash exists, it does not recompute it!
  [-d] store decoded image as PPM
  <FILE[S]> the file[s] to work on.

  With no options given, each raw with an accompanying hash will be decoded
  and compared to the existing hash. A summary of all errors/failed hash
  comparisons will be reported at the end.

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

} // namespace rstest

} // namespace rawspeed

using rawspeed::rstest::usage;
using rawspeed::rstest::process;
using rawspeed::rstest::results;

int main(int argc, char **argv) {

  auto hasFlag = [argc, argv](string flag) {
    bool found = false;
    for (int i = 1; i < argc; ++i) {
      if (!argv[i] || argv[i] != flag)
        continue;
      found = true;
      argv[i] = nullptr;
    }
    return found;
  };

  bool help = hasFlag("-h");
  bool create = hasFlag("-c");
  bool dump = hasFlag("-d");

  if (1 == argc || help)
    return usage(argv[0]);

  const CameraMetaData metadata(CMAKE_SOURCE_DIR "/data/cameras.xml");

  size_t time = 0;
  map<string, string> failedTests;
#ifdef _OPENMP
#pragma omp parallel for default(shared) schedule(static, 1) reduction(+ : time)
#endif
  for (int i = 1; i < argc; ++i) {
    if (!argv[i])
      continue;

    try {
      time += process(argv[i], &metadata, create, dump);
    } catch (RawspeedException& e) {
#ifdef _OPENMP
#pragma omp critical(io)
#endif
      {
        string msg = string(argv[i]) + " failed: " + e.what();
        cerr << msg << endl;
        failedTests.emplace(argv[i], msg);
      }
    }
  }

  cout << "Total decoding time: " << time / 1000.0 << "s" << endl << endl;

  return results(failedTests);
}
