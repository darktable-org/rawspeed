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
#include "common/Memory.h"                // for alignedFree, alignedMalloc...
#include "decoders/RawDecoderException.h" // for ThrowRDE, RawDecoderException
#include "io/IOException.h"               // for IOException
#include "parsers/TiffParserException.h"  // for TiffParserException
#include <algorithm>                      // for min
#include <cassert>                        // for assert
#include <cmath>                          // for NAN
#include <cstdlib>                        // for free
#include <cstring>                        // for memset, memcpy, strdup

using namespace std;

namespace RawSpeed {

RawImageData::RawImageData() : cfa(iPoint2D(0, 0)) {
  fill_n(blackLevelSeparate, 4, -1);
#ifdef HAVE_PTHREAD
  pthread_mutex_init(&mymutex, nullptr);
  pthread_mutex_init(&errMutex, nullptr);
  pthread_mutex_init(&mBadPixelMutex, nullptr);
#endif
}

RawImageData::RawImageData(const iPoint2D& _dim, uint32 _bpc, uint32 _cpp)
    : dim(_dim), isCFA(_cpp == 1), cfa(iPoint2D(0, 0)), cpp(_cpp),
      bpp(_bpc * _cpp) {
  fill_n(blackLevelSeparate, 4, -1);
  createData();
#ifdef HAVE_PTHREAD
  pthread_mutex_init(&mymutex, nullptr);
  pthread_mutex_init(&errMutex, nullptr);
  pthread_mutex_init(&mBadPixelMutex, nullptr);
#endif
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
#ifdef HAVE_PTHREAD
  pthread_mutex_destroy(&mymutex);
  pthread_mutex_destroy(&errMutex);
  pthread_mutex_destroy(&mBadPixelMutex);
#endif

  delete table;

  errors.clear();
  destroyData();
}


void RawImageData::createData() {
  if (dim.x > 65535 || dim.y > 65535)
    ThrowRDE("Dimensions too large for allocation.");
  if (dim.x <= 0 || dim.y <= 0)
    ThrowRDE("Dimension of one sides is less than 1 - cannot allocate image.");
  if (data)
    ThrowRDE("Duplicate data allocation in createData.");
  pitch = roundUp((size_t)dim.x * bpp, 16);
  data = (uchar8*)alignedMallocArray<16>(dim.y, pitch);
  if (!data)
    ThrowRDE("Memory Allocation failed.");
  uncropped_dim = dim;
}

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

uchar8* RawImageData::getData() {
  if (!data)
    ThrowRDE("Data not yet allocated.");
  return &data[mOffset.y*pitch+mOffset.x*bpp];
}

uchar8* RawImageData::getData(uint32 x, uint32 y) {
  if ((int)x >= dim.x)
    ThrowRDE("X Position outside image requested.");
  if ((int)y >= dim.y) {
    ThrowRDE("Y Position outside image requested.");
  }

  x += mOffset.x;
  y += mOffset.y;

  if (!data)
    ThrowRDE("Data not yet allocated.");

  return &data[y*pitch+x*bpp];
}

uchar8* RawImageData::getDataUncropped(uint32 x, uint32 y) {
  if ((int)x >= uncropped_dim.x)
    ThrowRDE("X Position outside image requested.");
  if ((int)y >= uncropped_dim.y) {
    ThrowRDE("Y Position outside image requested.");
  }

  if (!data)
    ThrowRDE("Data not yet allocated.");

  return &data[y*pitch+x*bpp];
}

iPoint2D __attribute__((pure)) RawSpeed::RawImageData::getUncroppedDim() const {
  return uncropped_dim;
}

iPoint2D __attribute__((pure)) RawImageData::getCropOffset() const {
  return mOffset;
}

void RawImageData::subFrame(iRectangle2D crop) {
  if (!crop.dim.isThisInside(dim - crop.pos)) {
    writeLog(DEBUG_PRIO_WARNING, "WARNING: RawImageData::subFrame - Attempted to create new subframe larger than original size. Crop skipped.\n");
    return;
  }
  if (crop.pos.x < 0 || crop.pos.y < 0 || !crop.hasPositiveArea()) {
    writeLog(DEBUG_PRIO_WARNING, "WARNING: RawImageData::subFrame - Negative crop offset. Crop skipped.\n");
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

void RawImageData::setError(const string& err) {
#ifdef HAVE_PTHREAD
  pthread_mutex_lock(&errMutex);
#endif
  errors.push_back(err);
#ifdef HAVE_PTHREAD
  pthread_mutex_unlock(&errMutex);
#endif
}

void RawImageData::createBadPixelMap()
{
  if (!isAllocated())
    ThrowRDE("(internal) Bad pixel map cannot be allocated before image.");
  mBadPixelMapPitch = roundUp(uncropped_dim.x / 8, 16);
  mBadPixelMap = (uchar8*)alignedMallocArray<16>(uncropped_dim.y, mBadPixelMapPitch);
  memset(mBadPixelMap, 0, (size_t)mBadPixelMapPitch * uncropped_dim.y);
  if (!mBadPixelMap)
    ThrowRDE("Memory Allocation failed.");
}

RawImage::RawImage(RawImageData* p) : p_(p) {
#ifdef HAVE_PTHREAD
  pthread_mutex_lock(&p_->mymutex);
#endif
  ++p_->dataRefCount;
#ifdef HAVE_PTHREAD
  pthread_mutex_unlock(&p_->mymutex);
#endif
}

RawImage::RawImage(const RawImage& p) : p_(p.p_) {
#ifdef HAVE_PTHREAD
  pthread_mutex_lock(&p_->mymutex);
#endif
  ++p_->dataRefCount;
#ifdef HAVE_PTHREAD
  pthread_mutex_unlock(&p_->mymutex);
#endif
}

RawImage::~RawImage() {
#ifdef HAVE_PTHREAD
  pthread_mutex_lock(&p_->mymutex);
#endif
  if (--p_->dataRefCount == 0) {
#ifdef HAVE_PTHREAD
    pthread_mutex_unlock(&p_->mymutex);
#endif
    delete p_;
    return;
  }
#ifdef HAVE_PTHREAD
  pthread_mutex_unlock(&p_->mymutex);
#endif
}

void RawImageData::copyErrorsFrom(const RawImage& other) {
  for (auto &error : other->errors) {
    setError(error);
  }
}

void RawImageData::transferBadPixelsToMap()
{
  if (mBadPixelPositions.empty())
    return;

  if (!mBadPixelMap)
    createBadPixelMap();

  for (unsigned int pos : mBadPixelPositions) {
    uint32 pos_x = pos&0xffff;
    uint32 pos_y = pos>>16;
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
  auto **workers = new RawImageWorker *[threads];
  int y_offset = 0;
  int y_per_thread = (height + threads - 1) / threads;

  for (int i = 0; i < threads; i++) {
    int y_end = min(y_offset + y_per_thread, height);
    workers[i] = new RawImageWorker(this, task, y_offset, y_end);
    workers[i]->startThread();
    y_offset = y_end;
  }
  for (int i = 0; i < threads; i++) {
    workers[i]->waitForThread();
    delete workers[i];
  }
  delete[] workers;
#else
  ThrowRDE("Unreachable");
#endif
}

void RawImageData::fixBadPixelsThread( int start_y, int end_y )
{
  int gw = (uncropped_dim.x + 15) / 32;
#ifdef __AFL_COMPILER
  int bad_count = 0;
#endif
  for (int y = start_y; y < end_y; y++) {
    auto *bad_map = (uint32 *)&mBadPixelMap[y * mBadPixelMapPitch];
    for (int x = 0 ; x < gw; x++) {
      // Test if there is a bad pixel within these 32 pixels
      if (bad_map[x] != 0) {
        auto *bad = (uchar8 *)&bad_map[x];
        // Go through each pixel
        for (int i = 0; i < 4; i++) {
          for (int j = 0; j < 8; j++) {
            if (1 == ((bad[i]>>j) & 1)) {
#ifdef __AFL_COMPILER
              if (bad_count++ > 100)
                ThrowRDE("The bad pixels are too damn high!");
#endif
              fixBadPixel(x*32+i*8+j, y, 0);
            }
          }
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
      memcpy(dst_pos, src_pos, (size_t)dim.x * bpp);
    }
  }
  if (validData.getBottom() < dim.y) {
    uchar8* src_pos = getData(0, validData.getBottom()-1);
    for (int y = validData.getBottom(); y < dim.y; y++ ) {
      uchar8* dst_pos = getData(0, y);
      memcpy(dst_pos, src_pos, (size_t)dim.x * bpp);
    }
  }
}

void RawImageData::clearArea( iRectangle2D area, uchar8 val /*= 0*/ )
{
  area = area.getOverlap(iRectangle2D(iPoint2D(0,0), dim));

  if (area.area() <= 0)
    return;

  for (int y = area.getTop(); y < area.getBottom(); y++)
    memset(getData(area.getLeft(), y), val, (size_t)area.getWidth() * bpp);
}

RawImage& RawImage::operator=(const RawImage& p) noexcept {
  if (this == &p)      // Same object?
    return *this;      // Yes, so skip assignment, and just return *this.
#ifdef HAVE_PTHREAD
  pthread_mutex_lock(&p_->mymutex);
#endif
  // Retain the old RawImageData before overwriting it
  RawImageData* const old = p_;
  p_ = p.p_;
  // Increment use on new data
  ++p_->dataRefCount;
  // If the RawImageData previously used by "this" is unused, delete it.
  if (--old->dataRefCount == 0) {
#ifdef HAVE_PTHREAD
    pthread_mutex_unlock(&(old->mymutex));
#endif
    delete old;
  } else {
#ifdef HAVE_PTHREAD
    pthread_mutex_unlock(&(old->mymutex));
#endif
  }
  return *this;
}

RawImage& RawImage::operator=(RawImage&& p) noexcept {
  operator=(p);
  return *this;
}

void *RawImageWorkerThread(void *_this) {
  auto *me = (RawImageWorker *)_this;
  me->performTask();
  return nullptr;
}

RawImageWorker::RawImageWorker( RawImageData *_img, RawImageWorkerTask _task, int _start_y, int _end_y )
{
  data = _img;
  start_y = _start_y;
  end_y = _end_y;
  task = _task;
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

void RawImageData::setTable( TableLookUp *t )
{
  delete table;

  table = t;
}

void RawImageData::setTable(const ushort16 *table_, int nfilled, bool dither) {
  assert(table_);
  assert(nfilled > 0);

  auto *t = new TableLookUp(1, dither);
  t->setTable(0, table_, nfilled);
  this->setTable(t);
}

const int TABLE_SIZE = 65536 * 2;

// Creates n numre of tables.
TableLookUp::TableLookUp( int _ntables, bool _dither ) : ntables(_ntables), dither(_dither) {
  tables = nullptr;
  if (ntables < 1) {
    ThrowRDE("Cannot construct 0 tables");
  }
  tables = new ushort16[ntables * TABLE_SIZE];
  memset(tables, 0, sizeof(ushort16) * ntables * TABLE_SIZE);
}

TableLookUp::~TableLookUp()
{
  delete[] tables;
  tables = nullptr;
}


void TableLookUp::setTable(int ntable, const ushort16 *table , int nfilled) {
  assert(table);
  assert(nfilled > 0);

  if (ntable > ntables) {
    ThrowRDE("Table lookup with number greater than number of tables.");
  }
  ushort16* t = &tables[ntable* TABLE_SIZE];
  if (!dither) {
    for (int i = 0; i < 65536; i++) {
      t[i] = (i < nfilled) ? table[i] : table[nfilled-1];
    }
    return;
  }
  for (int i = 0; i < nfilled; i++) {
    int center = table[i];
    int lower = i > 0 ? table[i-1] : center;
    int upper = i < (nfilled-1) ? table[i+1] : center;
    int delta = upper - lower;
    t[i*2] = center - ((upper - lower + 2) / 4);
    t[i*2+1] = delta;
  }

  for (int i = nfilled; i < 65536; i++) {
    t[i*2] = table[nfilled-1];
    t[i*2+1] = 0;
  }
  t[0] = t[1];
  t[TABLE_SIZE - 1] = t[TABLE_SIZE - 2];
}


ushort16* TableLookUp::getTable(int n) {
  if (n > ntables) {
    ThrowRDE("Table lookup with number greater than number of tables.");
  }
  return &tables[n * TABLE_SIZE];
}


} // namespace RawSpeed
