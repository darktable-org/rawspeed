/*
    This file is part of darktable,
    copyright (c) 2009--2011 johannes hanika.
    copyright (c) 2016 Pedro Côrte-Real

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "RawSpeed-API.h" // for RawImage, RawImageData, iPoint2D, CameraMe...

#include <array>      // for array
#include <cstddef>    // for size_t
#include <cstdint>    // for uint32_t, uint8_t, uint16_t
#include <cstdio>     // for fprintf, stdout, stderr
#include <memory>     // for unique_ptr, make_unique
#include <string>     // for string, operator+, basic_string
#include <sys/stat.h> // for stat
#include <vector>     // for vector

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX // do not want the min()/max() macros!
#endif

#include <Windows.h>
#endif

namespace rawspeed::identify {

std::string find_cameras_xml(const char* argv0);

std::string find_cameras_xml(const char *argv0) {
  struct stat statbuf;

#ifdef RS_CAMERAS_XML_PATH
  if (static const char* set_camfile = RS_CAMERAS_XML_PATH;
      stat(set_camfile, &statbuf)) {
    fprintf(stderr, "WARNING: Couldn't find cameras.xml in '%s'\n",
            set_camfile);
  } else {
    return set_camfile;
  }
#endif

  const std::string self(argv0);

  // If we haven't been provided with a valid cameras.xml path on compile try
  // relative to argv[0]
  const std::size_t lastslash = self.find_last_of(R"(/\)");
  const std::string bindir(self.substr(0, lastslash));

  std::string found_camfile(bindir +
                            "/../share/darktable/rawspeed/cameras.xml");

  if (stat(found_camfile.c_str(), &statbuf)) {
#ifndef __APPLE__
    fprintf(stderr, "WARNING: Couldn't find cameras.xml in '%s'\n",
            found_camfile.c_str());
#else
    fprintf(stderr, "WARNING: Couldn't find cameras.xml in '%s'\n",
            found_camfile.c_str());
    found_camfile =
        bindir + "/../Resources/share/darktable/rawspeed/cameras.xml";
    if (stat(found_camfile.c_str(), &statbuf)) {
      fprintf(stderr, "WARNING: Couldn't find cameras.xml in '%s'\n",
              found_camfile.c_str());
    }
#endif
  }

#ifdef RAWSPEED_STANDALONE_BUILD
  // running from build dir?
  found_camfile = std::string(RAWSPEED_SOURCE_DIR "/data/cameras.xml");
#endif

  if (stat(found_camfile.c_str(), &statbuf)) {
#ifndef __APPLE__
    fprintf(stderr, "ERROR: Couldn't find cameras.xml in '%s'\n",
            found_camfile.c_str());
    return {};
#else
    fprintf(stderr, "WARNING: Couldn't find cameras.xml in '%s'\n",
            found_camfile.c_str());
    found_camfile =
        bindir + "/../Resources/share/darktable/rawspeed/cameras.xml";
    if (stat(found_camfile.c_str(), &statbuf)) {
      fprintf(stderr, "ERROR: Couldn't find cameras.xml in '%s'\n",
              found_camfile.c_str());
      return {};
    }
#endif
  }

  return found_camfile;
}

} // namespace rawspeed::identify

using rawspeed::Buffer;
using rawspeed::CameraMetaData;
using rawspeed::FileReader;
using rawspeed::iPoint2D;
using rawspeed::RawParser;
using rawspeed::RawspeedException;
using rawspeed::identify::find_cameras_xml;

int main(int argc, char* argv[]) { // NOLINT

  if (argc != 2) {
    fprintf(stderr, "Usage: darktable-rs-identify <file>\n");
    return 0;
  }

  const std::string camfile = find_cameras_xml(argv[0]);
  if (camfile.empty()) {
    // fprintf(stderr, "ERROR: Couldn't find cameras.xml\n");
    return 2;
  }
  // fprintf(stderr, "Using cameras.xml from '%s'\n", camfile.c_str());

  try {
    std::unique_ptr<const CameraMetaData> meta;

#ifdef HAVE_PUGIXML
    meta = std::make_unique<CameraMetaData>(camfile.c_str());
#else
    meta = std::make_unique<CameraMetaData>();
#endif

    if (!meta) {
      fprintf(stderr, "ERROR: Couldn't get a CameraMetaData instance\n");
      return 2;
    }

#ifndef _WIN32
    char* imageFileName = argv[1];
#else
    // turn the locale ANSI encoded string into UTF-8 so that FileReader can
    // turn it into UTF-16 later
    int size = MultiByteToWideChar(CP_ACP, 0, argv[1], -1, nullptr, 0);
    std::wstring wImageFileName;
    wImageFileName.resize(size);
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, &wImageFileName[0], size);
    size = WideCharToMultiByte(CP_UTF8, 0, &wImageFileName[0], -1, nullptr, 0,
                               nullptr, nullptr);
    std::string _imageFileName;
    _imageFileName.resize(size);
    char* imageFileName = &_imageFileName[0];
    WideCharToMultiByte(CP_UTF8, 0, &wImageFileName[0], -1, imageFileName, size,
                        nullptr, nullptr);
#endif

    fprintf(stderr, "Loading file: \"%s\"\n", imageFileName);

    FileReader f(imageFileName);

    std::unique_ptr<const Buffer> m(f.readFile());

    RawParser t(*m);

    auto d(t.getDecoder(meta.get()));

    if (!d) {
      fprintf(stderr, "ERROR: Couldn't get a RawDecoder instance\n");
      return 2;
    }

    d->applyCrop = false;
    d->failOnUnknown = true;
    auto raw = d->mRaw.get(0);

    d->decodeMetaData(meta.get());

    fprintf(stdout, "make: %s\n", raw->metadata.make.c_str());
    fprintf(stdout, "model: %s\n", raw->metadata.model.c_str());

    fprintf(stdout, "canonical_make: %s\n", raw->metadata.canonical_make.c_str());
    fprintf(stdout, "canonical_model: %s\n",
            raw->metadata.canonical_model.c_str());
    fprintf(stdout, "canonical_alias: %s\n",
            raw->metadata.canonical_alias.c_str());

    d->checkSupport(meta.get());
    d->decodeRaw();
    d->decodeMetaData(meta.get());
    raw = d->mRaw.get(0);

    const auto errors = raw->getErrors();
    for (const auto& error : errors)
      fprintf(stderr, "WARNING: [rawspeed] %s\n", error.c_str());

    fprintf(stdout, "blackLevel: %d\n", raw->blackLevel);
    fprintf(stdout, "whitePoint: %d\n", raw->whitePoint);

    fprintf(stdout, "blackLevelSeparate: %d %d %d %d\n",
            raw->blackLevelSeparate[0], raw->blackLevelSeparate[1],
            raw->blackLevelSeparate[2], raw->blackLevelSeparate[3]);

    fprintf(stdout, "wbCoeffs: %f %f %f %f\n", raw->metadata.wbCoeffs[0],
            raw->metadata.wbCoeffs[1], raw->metadata.wbCoeffs[2],
            raw->metadata.wbCoeffs[3]);

    fprintf(stdout, "isCFA: %d\n", raw->isCFA);
    uint32_t filters = raw->cfa.getDcrawFilter();
    fprintf(stdout, "filters: %d (0x%x)\n", filters, filters);
    const uint32_t bpp = raw->getBpp();
    fprintf(stdout, "bpp: %d\n", bpp);
    const uint32_t cpp = raw->getCpp();
    fprintf(stdout, "cpp: %d\n", cpp);
    fprintf(stdout, "dataType: %u\n", static_cast<unsigned>(raw->getDataType()));

    // dimensions of uncropped image
    const iPoint2D dimUncropped = raw->getUncroppedDim();
    fprintf(stdout, "dimUncropped: %dx%d\n", dimUncropped.x, dimUncropped.y);

    // dimensions of cropped image
    iPoint2D dimCropped = raw->dim;
    fprintf(stdout, "dimCropped: %dx%d\n", dimCropped.x, dimCropped.y);

    // crop - Top,Left corner
    iPoint2D cropTL = raw->getCropOffset();
    fprintf(stdout, "cropOffset: %dx%d\n", cropTL.x, cropTL.y);

    fprintf(stdout, "fuji_rotation_pos: %d\n", raw->metadata.fujiRotationPos);
    fprintf(stdout, "pixel_aspect_ratio: %f\n", raw->metadata.pixelAspectRatio);

    double sum = 0.0F;
#ifdef HAVE_OPENMP
#pragma omp parallel for default(none) firstprivate(dimUncropped, raw, bpp) schedule(static) reduction(+ : sum)
#endif
    for (int y = 0; y < dimUncropped.y; ++y) {
      const uint8_t* const data = raw->getDataUncropped(0, y);

      for (unsigned x = 0; x < bpp * dimUncropped.x; ++x)
        sum += static_cast<double>(data[x]);
    }
    fprintf(stdout, "Image byte sum: %lf\n", sum);
    fprintf(stdout, "Image byte avg: %lf\n",
            sum / static_cast<double>(dimUncropped.y * dimUncropped.x * bpp));

    if (raw->getDataType() == rawspeed::RawImageType::F32) {
      sum = 0.0F;

#ifdef HAVE_OPENMP
#pragma omp parallel for default(none) firstprivate(dimUncropped, raw, cpp) schedule(static) reduction(+ : sum)
#endif
      for (int y = 0; y < dimUncropped.y; ++y) {
        const auto* const data =
            reinterpret_cast<float*>(raw->getDataUncropped(0, y));

        for (unsigned x = 0; x < cpp * dimUncropped.x; ++x)
          sum += static_cast<double>(data[x]);
      }

      fprintf(stdout, "Image float sum: %lf\n", sum);
      fprintf(stdout, "Image float avg: %lf\n",
              sum / static_cast<double>(dimUncropped.y * dimUncropped.x));
    } else if (raw->getDataType() == rawspeed::RawImageType::UINT16) {
      sum = 0.0F;

#ifdef HAVE_OPENMP
#pragma omp parallel for default(none) firstprivate(dimUncropped, raw, cpp) schedule(static) reduction(+ : sum)
#endif
      for (int y = 0; y < dimUncropped.y; ++y) {
        const auto* const data =
            reinterpret_cast<uint16_t*>(raw->getDataUncropped(0, y));

        for (unsigned x = 0; x < cpp * dimUncropped.x; ++x)
          sum += static_cast<double>(data[x]);
      }

      fprintf(stdout, "Image uint16_t sum: %lf\n", sum);
      fprintf(stdout, "Image uint16_t avg: %lf\n",
              sum / static_cast<double>(dimUncropped.y * dimUncropped.x));
    }
  } catch (const RawspeedException& e) {
    fprintf(stderr, "ERROR: [rawspeed] %s\n", e.what());

    /* if an exception is raised lets not retry or handle the
     specific ones, consider the file as corrupted */
    return 2;
  }

  return 0;
}

// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: indent-mode cstyle; replace-tabs on; tab-indents: off;
// kate: remove-trailing-space on;
