/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "rawspeedconfig.h"
#include "common/RawImage.h"
#include "adt/Casts.h"
#include "adt/CroppedArray2DRef.h"
#include "adt/Mutex.h"
#include "adt/Point.h"
#include "common/Common.h"
#include "common/TableLookUp.h"
#include "decoders/RawDecoderException.h"
#include "io/IOException.h"
#include "parsers/TiffParserException.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#ifdef DEBUG
#include "adt/Array2DRef.h"
#endif

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#include "AddressSanitizer.h"
#endif

namespace rawspeed {

void RawImageData::anchor() const {
  // Empty out-of-line definition for the purpose of anchoring
  // the class's vtable to this Translational Unit.
}

RawImageData::RawImageData(RawImageType type, const iPoint2D& _dim, int _bpc,
                           int _cpp)
    : dim(_dim), isCFA(_cpp == 1), dataType(type), cpp(_cpp) {
  assert(_bpc > 0);

  if (cpp > std::numeric_limits<decltype(cpp)>::max() / _bpc)
    ThrowRDE("Components-per-pixel is too large.");

  bpp = _bpc * _cpp;
  createData();
}

void RawImageData::createData() {
  static constexpr const auto alignment = 16;

  if (dim.x > 65535 || dim.y > 65535)
    ThrowRDE("Dimensions too large for allocation.");
  if (dim.x <= 0 || dim.y <= 0)
    ThrowRDE("Dimension of one sides is less than 1 - cannot allocate image.");
  if (cpp <= 0 || bpp <= 0)
    ThrowRDE("Unspecified component count - cannot allocate image.");
  if (isAllocated())
    ThrowRDE("Duplicate data allocation in createData.");

  // want each line to start at 16-byte aligned address
  pitch =
      implicit_cast<int>(roundUp(static_cast<size_t>(dim.x) * bpp, alignment));
  assert(isAligned(pitch, alignment));

#if defined(DEBUG) || __has_feature(address_sanitizer) ||                      \
    defined(__SANITIZE_ADDRESS__)
  // want to ensure that we have some padding
  pitch += alignment * alignment;
  assert(isAligned(pitch, alignment));
#endif

  padding = pitch - dim.x * bpp;

#if defined(DEBUG) || __has_feature(address_sanitizer) ||                      \
    defined(__SANITIZE_ADDRESS__)
  assert(padding > 0);
#endif

  data.resize(static_cast<size_t>(pitch) * dim.y);

  uncropped_dim = dim;

#ifndef NDEBUG
  const Array2DRef<std::byte> img = getByteDataAsUncroppedArray2DRef();

  for (int j = 0; j < dim.y; j++) {
    // each line is indeed 16-byte aligned
    assert(isAligned(&img(j, 0), alignment));
  }
#endif

  poisonPadding();
}

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
void RawImageData::poisonPadding() {
  if (padding <= 0)
    return;

  const Array2DRef<std::byte> img = getByteDataAsUncroppedArray2DRef();
  for (int j = 0; j < uncropped_dim.y; j++) {
    // and now poison the padding.
    ASan::PoisonMemoryRegion(img[j].end(), padding);
  }
}
#else
void RawImageData::poisonPadding() {
  // if we are building without ASAN, then there is no need/way to poison.
  // however, i think it is better to have such an empty function rather
  // than making this whole function not exist in ASAN-less builds
}
#endif

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
void RawImageData::unpoisonPadding() {
  if (padding <= 0)
    return;

  const Array2DRef<std::byte> img = getByteDataAsUncroppedArray2DRef();
  for (int j = 0; j < uncropped_dim.y; j++) {
    // and now unpoison the padding.
    ASan::UnPoisonMemoryRegion(img[j].end(), padding);
  }
}
#else
void RawImageData::unpoisonPadding() {
  // if we are building without ASAN, then there is no need/way to poison.
  // however, i think it is better to have such an empty function rather
  // than making this whole function not exist in ASAN-less builds
}
#endif

void RawImageData::setCpp(uint32_t val) {
  if (isAllocated())
    ThrowRDE("Attempted to set Components per pixel after data allocation");
  if (val > 4) {
    ThrowRDE(
        "Only up to 4 components per pixel is support - attempted to set: %d",
        val);
  }

  bpp /= cpp;
  cpp = val;
  bpp *= val;
}

iPoint2D RAWSPEED_READONLY rawspeed::RawImageData::getUncroppedDim() const {
  return uncropped_dim;
}

iPoint2D RAWSPEED_READONLY RawImageData::getCropOffset() const {
  return mOffset;
}

void RawImageData::subFrame(iRectangle2D crop) {
  if (!crop.hasPositiveArea())
    ThrowRDE("No positive crop area");

  if (!crop.dim.isThisInside(dim - crop.pos)) {
    writeLog(DEBUG_PRIO::WARNING, "WARNING: RawImageData::subFrame - Attempted "
                                  "to create new subframe larger than original "
                                  "size. Crop skipped.");
    return;
  }
  if (crop.pos.x < 0 || crop.pos.y < 0 || crop.dim.x < 0 || crop.dim.y < 0) {
    writeLog(DEBUG_PRIO::WARNING, "WARNING: RawImageData::subFrame - Negative "
                                  "crop offset. Crop skipped.");
    return;
  }

  // if CFA, and not X-Trans, adjust.
  if (isCFA && cfa.getDcrawFilter() != 1 && cfa.getDcrawFilter() != 9) {
    cfa.shiftRight(crop.pos.x);
    cfa.shiftDown(crop.pos.y);
  }

  mOffset += crop.pos;
  dim = crop.dim;
}

void RawImageData::createBadPixelMap() {
  if (!isAllocated())
    ThrowRDE("(internal) Bad pixel map cannot be allocated before image.");
  mBadPixelMapPitch = implicit_cast<uint32_t>(
      roundUp(roundUpDivisionSafe(uncropped_dim.x, 8), 16));
  assert(mBadPixelMap.empty());
  mBadPixelMap.resize(static_cast<size_t>(mBadPixelMapPitch) * uncropped_dim.y,
                      uint8_t(0));
}

void RawImageData::transferBadPixelsToMap() {
  MutexLocker guard(&mBadPixelMutex);
  if (mBadPixelPositions.empty())
    return;

  if (mBadPixelMap.empty())
    createBadPixelMap();

  for (unsigned int pos : mBadPixelPositions) {
    uint16_t pos_x = pos & 0xffff;
    uint16_t pos_y = pos >> 16;

    assert(pos_x < static_cast<uint16_t>(uncropped_dim.x));
    assert(pos_y < static_cast<uint16_t>(uncropped_dim.y));

    mBadPixelMap[mBadPixelMapPitch * pos_y + (pos_x >> 3)] |= 1 << (pos_x & 7);
  }
  mBadPixelPositions.clear();
}

void RawImageData::fixBadPixels() {
#if !defined(EMULATE_DCRAW_BAD_PIXELS)

  /* Transfer if not already done */
  transferBadPixelsToMap();

  /* Process bad pixels, if any */
  if (!mBadPixelMap.empty())
    startWorker(RawImageWorker::RawImageWorkerTask::FIX_BAD_PIXELS, false);

#else // EMULATE_DCRAW_BAD_PIXELS - not recommended, testing purposes only

  for (vector<uint32_t>::iterator i = mBadPixelPositions.begin();
       i != mBadPixelPositions.end(); ++i) {
    uint32_t pos = *i;
    uint32_t pos_x = pos & 0xffff;
    uint32_t pos_y = pos >> 16;
    uint32_t total = 0;
    uint32_t div = 0;
    // 0 side covered by unsignedness.
    for (uint32_t r = pos_x - 2;
         r <= pos_x + 2 && r < (uint32_t)uncropped_dim.x; r += 2) {
      for (uint32_t c = pos_y - 2;
           c <= pos_y + 2 && c < (uint32_t)uncropped_dim.y; c += 2) {
        uint16_t* pix = (uint16_t*)getDataUncropped(r, c);
        if (*pix) {
          total += *pix;
          div++;
        }
      }
    }
    uint16_t* pix = (uint16_t*)getDataUncropped(pos_x, pos_y);
    if (div) {
      pix[0] = total / div;
    }
  }
#endif
}

void RawImageData::startWorker(const RawImageWorker::RawImageWorkerTask task,
                               bool cropped) {
  const int height = [&]() {
    int h = cropped ? dim.y : uncropped_dim.y;
    if (static_cast<uint32_t>(task) &
        static_cast<uint32_t>(RawImageWorker::RawImageWorkerTask::FULL_IMAGE)) {
      h = uncropped_dim.y;
    }
    return h;
  }();

  const int threads = rawspeed_get_number_of_processor_cores();
  const int y_per_thread = (height + threads - 1) / threads;

#ifdef HAVE_OPENMP
#pragma omp parallel for default(none)                                         \
    firstprivate(threads, y_per_thread, height, task) num_threads(threads)     \
    schedule(static)
#endif
  for (int i = 0; i < threads; i++) {
    int y_offset = std::min(i * y_per_thread, height);
    int y_end = std::min((i + 1) * y_per_thread, height);

    RawImageWorker worker(this, task, y_offset, y_end);
  }
}

void RawImageData::fixBadPixelsThread(int start_y, int end_y) {
  int gw = (uncropped_dim.x + 15) / 32;

  const auto bad =
      Array2DRef(mBadPixelMap.data(), mBadPixelMapPitch, uncropped_dim.y);

  for (int y = start_y; y < end_y; y++) {
    for (int x = 0; x < gw; x++) {
      const auto block = bad[y].getBlock(32 / 8, x);

      // Test if there is a bad pixel within these 32 pixels
      if (std::all_of(block.begin(), block.end(),
                      [](const auto& val) { return val == 0; }))
        continue;

      // Go through each pixel
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 8; j++) {
          if (1 != ((block(i) >> j) & 1))
            continue;

          fixBadPixel(x * 32 + i * 8 + j, y, 0);
        }
      }
    }
  }
}

void RawImageData::clearArea(iRectangle2D area) {
  area = area.getOverlap(iRectangle2D(iPoint2D(0, 0), dim));

  if (area.area() <= 0)
    return;

  const CroppedArray2DRef<uint16_t> out = getU16DataAsCroppedArray2DRef();
  for (int y = area.getTop(); y < area.getBottom(); y++) {
    for (int x = area.getLeft(); x < area.getWidth() * cpp; ++x) {
      out(y, x) = 0;
    }
  }
}

RawImageWorker::RawImageWorker(RawImageData* _img, RawImageWorkerTask _task,
                               int _start_y, int _end_y) noexcept
    : data(_img), task(_task), start_y(_start_y), end_y(_end_y) {
  performTask();
}

void RawImageWorker::performTask() noexcept {
  try {
    switch (task) {
      using enum RawImageWorkerTask;
    case SCALE_VALUES:
      data->scaleValues(start_y, end_y);
      break;
    case FIX_BAD_PIXELS:
      data->fixBadPixelsThread(start_y, end_y);
      break;
    case APPLY_LOOKUP:
      data->doLookup(start_y, end_y);
      break;
    default:
      assert(false);
    }
  } catch (const RawDecoderException& e) {
    data->setError(e.what());
  } catch (const TiffParserException& e) {
    data->setError(e.what());
  } catch (const IOException& e) {
    data->setError(e.what());
  } catch (...) {
    // We should not get any other exception type here.
    __builtin_unreachable();
  }
}

void RawImageData::sixteenBitLookup() {
  if (table == nullptr) {
    return;
  }
  startWorker(RawImageWorker::RawImageWorkerTask::APPLY_LOOKUP, true);
}

void RawImageData::setTable(std::unique_ptr<TableLookUp> t) {
  table = std::move(t);
}

void RawImageData::setTable(const std::vector<uint16_t>& table_, bool dither) {
  assert(!table_.empty());

  auto t = std::make_unique<TableLookUp>(1, dither);
  t->setTable(0, table_);
  this->setTable(std::move(t));
}

} // namespace rawspeed
