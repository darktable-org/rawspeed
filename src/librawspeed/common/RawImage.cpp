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
#include "MemorySanitizer.h"              // for MSan
#include "common/Memory.h"                // for alignedFree, alignedMalloc...
#include "decoders/RawDecoderException.h" // for ThrowRDE, RawDecoderException
#include "io/IOException.h"               // for IOException
#include "parsers/TiffParserException.h"  // for TiffParserException
#include <algorithm>                      // for fill_n, min
#include <cassert>                        // for assert
#include <cmath>                          // for NAN
#include <cstdlib>                        // for size_t
#include <cstring>                        // for memcpy, memset
#include <limits>                         // for numeric_limits
#include <memory>                         // for unique_ptr, make_unique
#include <utility>                        // for move, swap

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
#include "AddressSanitizer.h" // for ASan::...
#endif

namespace rawspeed {

RawImageData::RawImageData() : cfa(iPoint2D(0, 0)) {
  blackLevelSeparate.fill(-1);
}

RawImageData::RawImageData(const iPoint2D& _dim, int _bpc, int _cpp)
    : dim(_dim), isCFA(_cpp == 1), cfa(iPoint2D(0, 0)), cpp(_cpp) {
  assert(_bpc > 0);

  if (cpp > std::numeric_limits<decltype(cpp)>::max() / _bpc)
    ThrowRDE("Components-per-pixel is too large.");

  bpp = _bpc * _cpp;
  blackLevelSeparate.fill(-1);
  createData();
}

RawImageData::~RawImageData() {
  mOffset = iPoint2D(0, 0);

  destroyData();
}


void RawImageData::createData() {
  static constexpr const auto alignment = 16;

  if (dim.x > 65535 || dim.y > 65535)
    ThrowRDE("Dimensions too large for allocation.");
  if (dim.x <= 0 || dim.y <= 0)
    ThrowRDE("Dimension of one sides is less than 1 - cannot allocate image.");
  if (data)
    ThrowRDE("Duplicate data allocation in createData.");

  // want each line to start at 16-byte aligned address
  pitch = roundUp(static_cast<size_t>(dim.x) * bpp, alignment);
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

  data = alignedMallocArray<uint8_t, alignment>(dim.y, pitch);

  if (!data)
    ThrowRDE("Memory Allocation failed.");

  uncropped_dim = dim;

#ifndef NDEBUG
  if (dim.y > 1) {
    // padding is the size of the area after last pixel of line n
    // and before the first pixel of line n+1
    assert(getData(dim.x - 1, 0) + bpp + padding == getData(0, 1));
  }

  for (int j = 0; j < dim.y; j++) {
    const uint8_t* const line = getData(0, j);
    // each line is indeed 16-byte aligned
    assert(isAligned(line, alignment));
  }
#endif

  poisonPadding();
}

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
void RawImageData::poisonPadding() const {
  if (padding <= 0)
    return;

  for (int j = 0; j < uncropped_dim.y; j++) {
    const uint8_t* const curr_line_end =
        getDataUncropped(uncropped_dim.x - 1, j) + bpp;

    // and now poison the padding.
    ASan::PoisonMemoryRegion(curr_line_end, padding);
  }
}
#else
void RawImageData::poisonPadding() const {
  // if we are building without ASAN, then there is no need/way to poison.
  // however, i think it is better to have such an empty function rather
  // than making this whole function not exist in ASAN-less builds
}
#endif

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
void RawImageData::unpoisonPadding() const {
  if (padding <= 0)
    return;

  for (int j = 0; j < uncropped_dim.y; j++) {
    const uint8_t* const curr_line_end =
        getDataUncropped(uncropped_dim.x - 1, j) + bpp;

    // and now unpoison the padding.
    ASan::UnPoisonMemoryRegion(curr_line_end, padding);
  }
}
#else
void RawImageData::unpoisonPadding() const {
  // if we are building without ASAN, then there is no need/way to poison.
  // however, i think it is better to have such an empty function rather
  // than making this whole function not exist in ASAN-less builds
}
#endif

void RawImageData::checkRowIsInitialized(int row) const {
  const auto rowsize = bpp * uncropped_dim.x;

  const uint8_t* const curr_line = getDataUncropped(0, row);

  // and check that image line is initialized.
  // do note that we are avoiding padding here.
  MSan::CheckMemIsInitialized(curr_line, rowsize);
}

#if __has_feature(memory_sanitizer) || defined(__SANITIZE_MEMORY__)
void RawImageData::checkMemIsInitialized() const {
  for (int j = 0; j < uncropped_dim.y; j++)
    checkRowIsInitialized(j);
}
#else
void RawImageData::checkMemIsInitialized() const {
  // While we could use the same version for non-MSAN build, even though it
  // does not do anything, i don't think it will be fully optimized away,
  // the getDataUncropped() call may still be there. To be re-evaluated.
}
#endif

void RawImageData::destroyData() {
  if (data)
    alignedFree(data);
  if (mBadPixelMap)
    alignedFree(mBadPixelMap);
  data = nullptr;
  mBadPixelMap = nullptr;
}

void RawImageData::setCpp(uint32_t val) {
  if (data)
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

uint8_t* RawImageData::getData() const {
  if (!data)
    ThrowRDE("Data not yet allocated.");
  return &data[mOffset.y*pitch+mOffset.x*bpp];
}

uint8_t* RawImageData::getData(uint32_t x, uint32_t y) {
  x += mOffset.x;
  y += mOffset.y;

  if (x >= static_cast<unsigned>(uncropped_dim.x))
    ThrowRDE("X Position outside image requested.");
  if (y >= static_cast<unsigned>(uncropped_dim.y))
    ThrowRDE("Y Position outside image requested.");

  if (!data)
    ThrowRDE("Data not yet allocated.");

  return &data[static_cast<size_t>(y) * pitch + x * bpp];
}

uint8_t* RawImageData::getDataUncropped(uint32_t x, uint32_t y) const {
  if (x >= static_cast<unsigned>(uncropped_dim.x))
    ThrowRDE("X Position outside image requested.");
  if (y >= static_cast<unsigned>(uncropped_dim.y))
    ThrowRDE("Y Position outside image requested.");

  if (!data)
    ThrowRDE("Data not yet allocated.");

  return &data[static_cast<size_t>(y) * pitch + x * bpp];
}

iPoint2D __attribute__((pure)) rawspeed::RawImageData::getUncroppedDim() const {
  return uncropped_dim;
}

iPoint2D __attribute__((pure)) RawImageData::getCropOffset() const {
  return mOffset;
}

void RawImageData::subFrame(iRectangle2D crop) {
  if (!crop.dim.isThisInside(dim - crop.pos)) {
    writeLog(DEBUG_PRIO::WARNING, "WARNING: RawImageData::subFrame - Attempted "
                                  "to create new subframe larger than original "
                                  "size. Crop skipped.");
    return;
  }
  if (crop.pos.x < 0 || crop.pos.y < 0 || !crop.hasPositiveArea()) {
    writeLog(DEBUG_PRIO::WARNING, "WARNING: RawImageData::subFrame - Negative "
                                  "crop offset. Crop skipped.");
    return;
  }

  // if CFA, and not X-Trans, adjust.
  if (isCFA && cfa.getDcrawFilter() != 1 && cfa.getDcrawFilter() != 9) {
    cfa.shiftLeft(crop.pos.x);
    cfa.shiftDown(crop.pos.y);
  }

  mOffset += crop.pos;
  dim = crop.dim;
}

void RawImageData::createBadPixelMap()
{
  if (!isAllocated())
    ThrowRDE("(internal) Bad pixel map cannot be allocated before image.");
  mBadPixelMapPitch = roundUp(roundUpDivision(uncropped_dim.x, 8), 16);
  mBadPixelMap =
      alignedMallocArray<uint8_t, 16>(uncropped_dim.y, mBadPixelMapPitch);
  memset(mBadPixelMap, 0,
         static_cast<size_t>(mBadPixelMapPitch) * uncropped_dim.y);
  if (!mBadPixelMap)
    ThrowRDE("Memory Allocation failed.");
}

RawImage::RawImage(RawImageData* p) : p_(p) {

}

RawImage::RawImage(const RawImage& p) : p_(p.p_) {

}



void RawImageData::transferBadPixelsToMap()
{
  MutexLocker guard(&mBadPixelMutex);
  if (mBadPixelPositions.empty())
    return;

  if (!mBadPixelMap)
    createBadPixelMap();

  for (unsigned int pos : mBadPixelPositions) {
    uint16_t pos_x = pos & 0xffff;
    uint16_t pos_y = pos >> 16;

    assert(pos_x < static_cast<uint16_t>(uncropped_dim.x));
    assert(pos_y < static_cast<uint16_t>(uncropped_dim.y));

    mBadPixelMap[mBadPixelMapPitch * pos_y + (pos_x >> 3)] |= 1 << (pos_x&7);
  }
  mBadPixelPositions.clear();
}

void RawImageData::fixBadPixels()
{
#if !defined (EMULATE_DCRAW_BAD_PIXELS)

  /* Transfer if not already done */
  transferBadPixelsToMap();

#if 0 // For testing purposes
  if (!mBadPixelMap)
    createBadPixelMap();
  for (int y = 400; y < 700; y++){
    for (int x = 1200; x < 1700; x++) {
      mBadPixelMap[mBadPixelMapPitch * y + (x >> 3)] |= 1 << (x&7);
    }
  }
#endif

  /* Process bad pixels, if any */
  if (mBadPixelMap)
    startWorker(RawImageWorker::RawImageWorkerTask::FIX_BAD_PIXELS, false);

#else  // EMULATE_DCRAW_BAD_PIXELS - not recommended, testing purposes only

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

  for (int y = start_y; y < end_y; y++) {
    const auto* bad_map =
        reinterpret_cast<const uint32_t*>(&mBadPixelMap[y * mBadPixelMapPitch]);
    for (int x = 0; x < gw; x++) {
      // Test if there is a bad pixel within these 32 pixels
      if (bad_map[x] == 0)
        continue;
      const auto* bad = reinterpret_cast<const uint8_t*>(&bad_map[x]);
      // Go through each pixel
      for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 8; j++) {
          if (1 != ((bad[i] >> j) & 1))
            continue;

          fixBadPixel(x * 32 + i * 8 + j, y, 0);
        }
      }
    }
  }
}

void RawImageData::blitFrom(const RawImage& src, const iPoint2D& srcPos,
                            const iPoint2D& size, const iPoint2D& destPos) {
  iRectangle2D src_rect(srcPos, size);
  iRectangle2D dest_rect(destPos, size);
  src_rect = src_rect.getOverlap(iRectangle2D(iPoint2D(0,0), src->dim));
  dest_rect = dest_rect.getOverlap(iRectangle2D(iPoint2D(0,0), dim));

  iPoint2D blitsize = src_rect.dim.getSmallest(dest_rect.dim);
  if (blitsize.area() <= 0)
    return;

  // TODO: Move offsets after crop.
  copyPixels(getData(dest_rect.pos.x, dest_rect.pos.y), pitch,
             src->getData(src_rect.pos.x, src_rect.pos.y), src->pitch,
             blitsize.x * bpp, blitsize.y);
}

/* Does not take cfa into consideration */
void RawImageData::expandBorder(iRectangle2D validData)
{
  validData = validData.getOverlap(iRectangle2D(0,0,dim.x, dim.y));
  if (validData.pos.x > 0) {
    for (int y = 0; y < dim.y; y++ ) {
      const uint8_t* src_pos = getData(validData.pos.x, y);
      uint8_t* dst_pos = getData(validData.pos.x - 1, y);
      for (int x = validData.pos.x; x >= 0; x--) {
        for (int i = 0; i < bpp; i++) {
          dst_pos[i] = src_pos[i];
        }
        dst_pos -= bpp;
      }
    }
  }

  if (validData.getRight() < dim.x) {
    int pos = validData.getRight();
    for (int y = 0; y < dim.y; y++ ) {
      const uint8_t* src_pos = getData(pos - 1, y);
      uint8_t* dst_pos = getData(pos, y);
      for (int x = pos; x < dim.x; x++) {
        for (int i = 0; i < bpp; i++) {
          dst_pos[i] = src_pos[i];
        }
        dst_pos += bpp;
      }
    }
  }

  if (validData.pos.y > 0) {
    const uint8_t* src_pos = getData(0, validData.pos.y);
    for (int y = 0; y < validData.pos.y; y++ ) {
      uint8_t* dst_pos = getData(0, y);
      memcpy(dst_pos, src_pos, static_cast<size_t>(dim.x) * bpp);
    }
  }
  if (validData.getBottom() < dim.y) {
    const uint8_t* src_pos = getData(0, validData.getBottom() - 1);
    for (int y = validData.getBottom(); y < dim.y; y++ ) {
      uint8_t* dst_pos = getData(0, y);
      memcpy(dst_pos, src_pos, static_cast<size_t>(dim.x) * bpp);
    }
  }
}

void RawImageData::clearArea(iRectangle2D area, uint8_t val /*= 0*/) {
  area = area.getOverlap(iRectangle2D(iPoint2D(0,0), dim));

  if (area.area() <= 0)
    return;

  for (int y = area.getTop(); y < area.getBottom(); y++)
    memset(getData(area.getLeft(), y), val,
           static_cast<size_t>(area.getWidth()) * bpp);
}

RawImage& RawImage::operator=(RawImage&& rhs) noexcept {
  if (this == &rhs)
    return *this;

  std::swap(p_, rhs.p_);

  return *this;
}

RawImage& RawImage::operator=(const RawImage& rhs) noexcept {
  if (this == &rhs)
    return *this;

  RawImage tmp(rhs);
  *this = std::move(tmp);

  return *this;
}

RawImageWorker::RawImageWorker(RawImageData* _img, RawImageWorkerTask _task,
                               int _start_y, int _end_y) noexcept
    : data(_img), task(_task), start_y(_start_y), end_y(_end_y) {
  performTask();
}

void RawImageWorker::performTask() noexcept {
  try {
    switch(task)
    {
    case RawImageWorkerTask::SCALE_VALUES:
      data->scaleValues(start_y, end_y);
      break;
    case RawImageWorkerTask::FIX_BAD_PIXELS:
      data->fixBadPixelsThread(start_y, end_y);
      break;
    case RawImageWorkerTask::APPLY_LOOKUP:
      data->doLookup(start_y, end_y);
      break;
    default:
      // NOLINTNEXTLINE: https://bugs.llvm.org/show_bug.cgi?id=50532
      assert(false);
    }
  } catch (const RawDecoderException& e) {
    data->setError(e.what());
  } catch (const TiffParserException& e) {
    data->setError(e.what());
  } catch (const IOException& e) {
    data->setError(e.what());
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
