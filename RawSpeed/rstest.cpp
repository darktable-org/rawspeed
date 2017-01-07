/*
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

#include "config.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "RawSpeed-API.h"
#include <stdio.h>

using namespace std;
using namespace RawSpeed;

std::string md5_hash(const uint8_t *message, size_t len);

struct Timer {
  mutable chrono::steady_clock::time_point start = chrono::steady_clock::now();
  size_t operator()() const {
    auto ms = chrono::duration_cast<chrono::milliseconds>(
                  chrono::steady_clock::now() - start)
                  .count();
    start = chrono::steady_clock::now();
    return ms;
  }
};

string img_hash(RawImage &r) {
  ostringstream oss;
  char line[256];

#define APPEND(...)                                                            \
  do {                                                                         \
    snprintf(line, sizeof(line), __VA_ARGS__);                                 \
    oss << line;                                                               \
  } while (0)

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
  APPEND("pitch: %d\n", r->pitch);

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

  // clear the bytes at the end of each line, so we get a consistent hash
  size_t padding = r->pitch - dimUncropped.x * r->getBpp();
  if (padding) {
    uint8_t *d = r->getDataUncropped(0, 1) - padding;
    for (int i = 0; i < r->getUncroppedDim().y; ++i, d += r->pitch)
      memset(d, 0, padding);
  }
  string hash =
      md5_hash(r->getDataUncropped(0, 0), r->pitch * r->getUncroppedDim().y);
  APPEND("data md5sum: %s\n", hash.c_str());

  for (const char *e : r->errors)
    APPEND("WARNING: [rawspeed] %s\n", e);

#undef APPEND

  return oss.str();
}

void writePPM(RawImage raw, string fn) {
  FILE *f = fopen(fn.c_str(), "wb");

  int width = raw->dim.x;
  int height = raw->dim.y;

  // Write PPM header
  fprintf(f, "P5\n%d %d\n65535\n", width, height);

  // Write pixels
  for (int y = 0; y < height; ++y) {
    unsigned short *row = (unsigned short *)raw->getData(0, y);
    // Swap for PPM format byte ordering
    if (getHostEndianness() == little)
      for (int x = 0; x < width; ++x)
        row[x] = __builtin_bswap16(row[x]);

    fwrite(row, 2, width, f);
  }
  fclose(f);
}

size_t process(string filename, CameraMetaData *metadata, bool create,
               bool dump) {

  FileReader reader((LPCWSTR)filename.c_str());
  unique_ptr<FileMap> map = unique_ptr<FileMap>(reader.readFile());
  // FileMap* map = readFile( argv[1] );

  Timer t;

  RawParser parser(map.get());
  unique_ptr<RawDecoder> decoder =
      unique_ptr<RawDecoder>(parser.getDecoder(metadata));
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
    ofstream f(filename + ".hash");
    f << img_hash(raw);
    if (dump)
      writePPM(raw, filename + ".ppm");
  } else {
    ifstream in(filename + ".hash");
    if (in) {
      string truth((istreambuf_iterator<char>(in)),
                   istreambuf_iterator<char>());
      string h = img_hash(raw);
      if (h != truth) {
        ofstream f(filename + ".hash.failed");
        f << h;
        if (dump)
          writePPM(raw, filename + ".failed.ppm");
        throw std::runtime_error("hash/metadata mismatch");
      }
    } else
      cerr << filename << ".hash missing." << endl;
  }

  return time;
}

int main(int argc, char **argv) {

  CameraMetaData metadata(CMAKE_SOURCE_DIR "/data/cameras.xml");

  int j = 1;
  auto hasFlag = [&](string flag) {
    bool ret = argv[j] == flag;
    j += ret ? 1 : 0;
    return ret;
  };
  bool create = hasFlag("-c");
  bool dump = hasFlag("-d");

  size_t time = 0;
  vector<string> failedTests;
#ifdef _OPENMP
#pragma omp parallel for default(shared) schedule(static, 1) reduction(+ : time)
#endif
  for (int i = j; i < argc; ++i) {
    try {
      time += process(argv[i], &metadata, create, dump);
    } catch (std::runtime_error e) {
#ifdef _OPENMP
#pragma omp critical(io)
#endif
      {
        failedTests.push_back(string(argv[i]) + " failed: " + e.what());
        cerr << failedTests.back() << endl;
      }
    }
  }

  cout << "Total decoding time: " << time / 1000.0 << "s" << endl;

  if (!failedTests.empty()) {
    cerr << "WARNING: the following tests have failed:\n";
    for (auto i : failedTests)
      cerr << i << "\n";
  }

  return failedTests.empty() ? 0 : 1;
}
