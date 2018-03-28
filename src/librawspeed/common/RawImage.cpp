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

#include "rawspeedconfig.h" // for WITH_SSE2
#include "common/RawImage.h"
#include "MemorySanitizer.h"              // for MSan::CheckMemIsInitialized
#include "common/Memory.h"                // for alignedFree, alignedMalloc...
#include "decoders/RawDecoderException.h" // for ThrowRDE, RawDecoderException
#include "io/IOException.h"               // for IOException
#include "parsers/TiffParserException.h"  // for TiffParserException
#include <algorithm>                      // for min
#include <cassert>                        // for assert
#include <cmath>                          // for NAN
#include <cstdlib>                        // for free
#include <cstring>                        // for memset, memcpy, strdup
#include <limits>                         // for numeric_limits
#include <memory>                         // for unique_ptr

using std::fill_n;
using std::string;

namespace rawspeed {

RawImageData::RawImageData() : cfa(iPoint2D(0, 0)) {
  blackLevelSeparate.fill(-1);
}

RawImageData::RawImageData(const iPoint2D& _dim, uint32 _bpc, uint32 _cpp)
    : dim(_dim), isCFA(_cpp == 1), cfa(iPoint2D(0, 0)), cpp(_cpp) {
  assert(_bpc > 0);

  if (cpp > std::numeric_limits<decltype(bpp)>::max() / _bpc)
    ThrowRDE("Components-per-pixel is too large.");

  bpp = _bpc * _cpp;
  blackLevelSeparate.fill(-1);
  createData();
}

ImageMetaData::ImageMetaData() {
  subsampling.x = subsampling.y = 1;
  isoSpeed = 0;
  pixelAspectRatio = 1;
  fujiRotationPos = 0;
  fill_n(wbCoeffs, 4, NAN);
}

RawImageData::~RawImageData() {
  assert(dataRefCount == 0);
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

  data = alignedMallocArray<uchar8, alignment>(dim.y, pitch);

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
    const uchar8* const line = getData(0, j);
    // each line is indeed 16-byte aligned
    assert(isAligned(line, alignment));
  }
#endif

  poisonPadding();
}

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
void RawImageData::poisonPadding() {
  if (padding <= 0)
    return;

  for (int j = 0; j < uncropped_dim.y; j++) {
    const uchar8* const curr_line_end =
        getDataUncropped(uncropped_dim.x - 1, j) + bpp;

    // and now poison the padding.
    ASAN_POISON_MEMORY_REGION(curr_line_end, padding);
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

  for (int j = 0; j < uncropped_dim.y; j++) {
    const uchar8* const curr_line_end =
        getDataUncropped(uncropped_dim.x - 1, j) + bpp;

    // and now unpoison the padding.
    ASAN_UNPOISON_MEMORY_REGION(curr_line_end, padding);
  }
}
#else
void RawImageData::unpoisonPadding() {
  // if we are building without ASAN, then there is no need/way to poison.
  // however, i think it is better to have such an empty function rather
  // than making this whole function not exist in ASAN-less builds
}
#endif

void RawImageData::checkRowIsInitialized(int row) {
  const auto rowsize = bpp * uncropped_dim.x;

  const uchar8* const curr_line = getDataUncropped(0, row);

  // and check that image line is initialized.
  // do note that we are avoiding padding here.
  MSan::CheckMemIsInitialized(curr_line, rowsize);
}

#if __has_feature(memory_sanitizer) || defined(__SANITIZE_MEMORY__)
void RawImageData::checkMemIsInitialized() {
  for (int j = 0; j < uncropped_dim.y; j++)
    checkRowIsInitialized(j);
}
#else
void RawImageData::checkMemIsInitialized() {
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

void RawImageData::setCpp(uint32 val) {
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

uchar8* RawImageData::getData() const {
  if (!data)
    ThrowRDE("Data not yet allocated.");
  return &data[mOffset.y*pitch+mOffset.x*bpp];
}

uchar8* RawImageData::getData(uint32 x, uint32 y) {
  if (x >= static_cast<unsigned>(uncropped_dim.x))
    ThrowRDE("X Position outside image requested.");
  if (y >= static_cast<unsigned>(uncropped_dim.y))
    ThrowRDE("Y Position outside image requested.");

  x += mOffset.x;
  y += mOffset.y;

  if (!data)
    ThrowRDE("Data not yet allocated.");

  return &data[static_cast<size_t>(y) * pitch + x * bpp];
}

uchar8* RawImageData::getDataUncropped(uint32 x, uint32 y) {
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
    writeLog(DEBUG_PRIO_WARNING, "WARNING: RawImageData::subFrame - Attempted "
                                 "to create new subframe larger than original "
                                 "size. Crop skipped.");
    return;
  }
  if (crop.pos.x < 0 || crop.pos.y < 0 || !crop.hasPositiveArea()) {
    writeLog(DEBUG_PRIO_WARNING, "WARNING: RawImageData::subFrame - Negative "
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
      alignedMallocArray<uchar8, 16>(uncropped_dim.y, mBadPixelMapPitch);
  memset(mBadPixelMap, 0,
         static_cast<size_t>(mBadPixelMapPitch) * uncropped_dim.y);
  if (!mBadPixelMap)
    ThrowRDE("Memory Allocation failed.");
}

RawImage::RawImage(RawImageData* p) : p_(p) {
  MutexLocker guard(&p_->mymutex);
  ++p_->dataRefCount;
}

RawImage::RawImage(const RawImage& p) : p_(p.p_) {
  MutexLocker guard(&p_->mymutex);
  ++p_->dataRefCount;
}

RawImage::~RawImage() {
  p_->mymutex.Lock();

  --p_->dataRefCount;

  if (p_->dataRefCount == 0) {
    p_->mymutex.Unlock();
    delete p_;
    return;
  }

  p_->mymutex.Unlock();
}

void RawImageData::transferBadPixelsToMap()
{
  MutexLocker guard(&mBadPixelMutex);
  if (mBadPixelPositions.empty())
    return;

  if (!mBadPixelMap)
    createBadPixelMap();

  for (unsigned int pos : mBadPixelPositions) {
    ushort16 pos_x = pos & 0xffff;
    ushort16 pos_y = pos >> 16;

    assert(pos_x < static_cast<ushort16>(uncropped_dim.x));
    assert(pos_y < static_cast<ushort16>(uncropped_dim.y));

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
    startWorker(RawImageWorker::FIX_BAD_PIXELS, false);

#else  // EMULATE_DCRAW_BAD_PIXELS - not recommended, testing purposes only

  for (vector<uint32>::iterator i=mBadPixelPositions.begin(); i != mBadPixelPositions.end(); ++i) {
    uint32 pos = *i;
    uint32 pos_x = pos&0xffff;
    uint32 pos_y = pos>>16;
    uint32 total = 0;
    uint32 div = 0;
    // 0 side covered by unsignedness.
    for (uint32 r=pos_x-2; r<=pos_x+2 && r<(uint32)uncropped_dim.x; r+=2) {
      for (uint32 c=pos_y-2; c<=pos_y+2 && c<(uint32)uncropped_dim.y; c+=2) {
        ushort16* pix = (ushort16*)getDataUncropped(r,c);
        if (*pix) {
          total += *pix;
          div++;
        }
      }
    }
    ushort16* pix = (ushort16*)getDataUncropped(pos_x,pos_y);
    if (div) {
      pix[0] = total / div;
    }
  }
#endif

}

void RawImageData::startWorker(RawImageWorker::RawImageWorkerTask task, bool cropped )
{
  int height = (cropped) ? dim.y : uncropped_dim.y;
  if (task & RawImageWorker::FULL_IMAGE) {
    height = uncropped_dim.y;
  }

  int threads = getThreadCount();
  if (threads <= 1) {
    RawImageWorker worker(this, task, 0, height);
    worker.performTask();
    return;
  }

#ifdef HAVE_PTHREAD
  std::vector<RawImageWorker> workers;
  workers.reserve(threads);

  int y_offset = 0;
  int y_per_thread = (height + threads - 1) / threads;

  for (int i = 0; i < threads; i++) {
    int y_end = std::min(y_offset + y_per_thread, height);

    workers.emplace_back(this, task, y_offset, y_end);
    workers.back().startThread();

    y_offset = y_end;
  }

  for (auto& worker : workers)
    worker.waitForThread();
#else
  ThrowRDE("Unreachable");
#endif
}

void RawImageData::fixBadPixelsThread(int start_y, int end_y) {
  int gw = (uncropped_dim.x + 15) / 32;

  for (int y = start_y; y < end_y; y++) {
    auto* bad_map =
        reinterpret_cast<const uint32*>(&mBadPixelMap[y * mBadPixelMapPitch]);
    for (int x = 0; x < gw; x++) {
      // Test if there is a bad pixel within these 32 pixels
      if (bad_map[x] == 0)
        continue;
      auto* bad = reinterpret_cast<const uchar8*>(&bad_map[x]);
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
      uchar8* src_pos = getData(validData.pos.x, y);
      uchar8* dst_pos = getData(validData.pos.x-1, y);
      for (int x = validData.pos.x; x >= 0; x--) {
        for (uint32 i = 0; i < bpp; i++) {
          dst_pos[i] = src_pos[i];
        }
        dst_pos -= bpp;
      }
    }
  }

  if (validData.getRight() < dim.x) {
    int pos = validData.getRight();
    for (int y = 0; y < dim.y; y++ ) {
      uchar8* src_pos = getData(pos-1, y);
      uchar8* dst_pos = getData(pos, y);
      for (int x = pos; x < dim.x; x++) {
        for (uint32 i = 0; i < bpp; i++) {
          dst_pos[i] = src_pos[i];
        }
        dst_pos += bpp;
      }
    }
  }

  if (validData.pos.y > 0) {
    uchar8* src_pos = getData(0, validData.pos.y);
    for (int y = 0; y < validData.pos.y; y++ ) {
      uchar8* dst_pos = getData(0, y);
      memcpy(dst_pos, src_pos, static_cast<size_t>(dim.x) * bpp);
    }
  }
  if (validData.getBottom() < dim.y) {
    uchar8* src_pos = getData(0, validData.getBottom()-1);
    for (int y = validData.getBottom(); y < dim.y; y++ ) {
      uchar8* dst_pos = getData(0, y);
      memcpy(dst_pos, src_pos, static_cast<size_t>(dim.x) * bpp);
    }
  }
}

void RawImageData::clearArea( iRectangle2D area, uchar8 val /*= 0*/ )
{
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

void *RawImageWorkerThread(void *_this) {
  auto* me = static_cast<RawImageWorker*>(_this);
  me->performTask();
  return nullptr;
}

RawImageWorker::RawImageWorker(RawImageData* _img, RawImageWorkerTask _task,
                               int _start_y, int _end_y)
    : data(_img), task(_task), start_y(_start_y), end_y(_end_y) {
#ifdef HAVE_PTHREAD
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
#endif
}

#ifdef HAVE_PTHREAD
RawImageWorker::~RawImageWorker() { pthread_attr_destroy(&attr); }
#endif

#ifdef HAVE_PTHREAD
void RawImageWorker::startThread()
{
  /* Initialize and set thread detached attribute */
  pthread_create(&threadid, &attr, RawImageWorkerThread, this);
}

void RawImageWorker::waitForThread()
{
  void *status;
  pthread_join(threadid, &status);
}
#endif

void RawImageWorker::performTask()
{
  try {
    switch(task)
    {
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
  } catch (RawDecoderException &e) {
    data->setError(e.what());
  } catch (TiffParserException &e) {
    data->setError(e.what());
  } catch (IOException &e) {
    data->setError(e.what());
  }
}

void RawImageData::sixteenBitLookup() {
  if (table == nullptr) {
    return;
  }
  startWorker(RawImageWorker::APPLY_LOOKUP, true);
}

void RawImageData::setTable(std::unique_ptr<TableLookUp> t) {
  table = std::move(t);
}

void RawImageData::setTable(const std::vector<ushort16>& table_, bool dither) {
  assert(!table_.empty());

  auto t = std::make_unique<TableLookUp>(1, dither);
  t->setTable(0, table_);
  this->setTable(std::move(t));
}

} // namespace rawspeed
