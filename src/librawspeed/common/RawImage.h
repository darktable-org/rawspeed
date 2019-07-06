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

#pragma once

#include "rawspeedconfig.h"
#include "ThreadSafetyAnalysis.h"      // for GUARDED_BY, REQUIRES
#include "common/Common.h"             // for uint32, uchar8, ushort16, wri...
#include "common/ErrorLog.h"           // for ErrorLog
#include "common/Mutex.h"              // for Mutex
#include "common/Point.h"              // for iPoint2D, iRectangle2D (ptr o...
#include "common/TableLookUp.h"        // for TableLookUp
#include "metadata/BlackArea.h"        // for BlackArea
#include "metadata/ColorFilterArray.h" // for ColorFilterArray
#include <array>                       // for array
#include <memory>                      // for unique_ptr, operator==
#include <string>                      // for string
#include <vector>                      // for vector

namespace rawspeed {

class RawImage;

class RawImageData;

enum RawImageType { TYPE_USHORT16, TYPE_FLOAT32 };

class RawImageWorker {
public:
  enum RawImageWorkerTask {
    SCALE_VALUES = 1, FIX_BAD_PIXELS = 2, APPLY_LOOKUP = 3 | 0x1000, FULL_IMAGE = 0x1000
  };

private:
  RawImageData* data;
  RawImageWorkerTask task;
  int start_y;
  int end_y;

  void performTask() noexcept;

public:
  RawImageWorker(RawImageData* img, RawImageWorkerTask task, int start_y,
                 int end_y) noexcept;
};

class ImageMetaData {
public:
  ImageMetaData();

  // Aspect ratio of the pixels, usually 1 but some cameras need scaling
  // <1 means the image needs to be stretched vertically, (0.5 means 2x)
  // >1 means the image needs to be stretched horizontally (2 mean 2x)
  double pixelAspectRatio;

  // White balance coefficients of the image
  std::array<float, 4> wbCoeffs;

  // How many pixels far down the left edge and far up the right edge the image
  // corners are when the image is rotated 45 degrees in Fuji rotated sensors.
  uint32 fujiRotationPos;

  // Fuji RAW exposure offset compared to camera-produced JPEGs.
  float fujiExposureBias;

  iPoint2D subsampling;
  std::string make;
  std::string model;
  std::string mode;

  std::string canonical_make;
  std::string canonical_model;
  std::string canonical_alias;
  std::string canonical_id;

  // ISO speed. If known the value is set, otherwise it will be '0'.
  int isoSpeed;
};

class RawImageData : public ErrorLog {
  friend class RawImageWorker;
public:
  virtual ~RawImageData();
  uint32 getCpp() const { return cpp; }
  uint32 getBpp() const { return bpp; }
  void setCpp(uint32 val);
  void createData();
  void poisonPadding();
  void unpoisonPadding();
  void checkRowIsInitialized(int row);
  void checkMemIsInitialized();
  void destroyData();
  void blitFrom(const RawImage& src, const iPoint2D& srcPos,
                const iPoint2D& size, const iPoint2D& destPos);
  rawspeed::RawImageType getDataType() const { return dataType; }
  uchar8* getData() const;
  uchar8* getData(uint32 x, uint32 y);    // Not super fast, but safe. Don't use per pixel.
  uchar8* getDataUncropped(uint32 x, uint32 y);
  void subFrame(iRectangle2D cropped);
  void clearArea(iRectangle2D area, uchar8 value = 0);
  iPoint2D __attribute__((pure)) getUncroppedDim() const;
  iPoint2D __attribute__((pure)) getCropOffset() const;
  virtual void scaleBlackWhite() = 0;
  virtual void calculateBlackAreas() = 0;
  virtual void setWithLookUp(ushort16 value, uchar8* dst, uint32* random) = 0;
  void sixteenBitLookup();
  void transferBadPixelsToMap() REQUIRES(!mBadPixelMutex);
  void fixBadPixels() REQUIRES(!mBadPixelMutex);
  void expandBorder(iRectangle2D validData);
  void setTable(const std::vector<ushort16>& table_, bool dither);
  void setTable(std::unique_ptr<TableLookUp> t);

  bool isAllocated() {return !!data;}
  void createBadPixelMap();
  iPoint2D dim;
  uint32 pitch = 0;

  // padding is the size of the area after last pixel of line n
  // and before the first pixel of line n+1
  uint32 padding = 0;

  bool isCFA{true};
  ColorFilterArray cfa;
  int blackLevel = -1;
  std::array<int, 4> blackLevelSeparate;
  int whitePoint = 65536;
  std::vector<BlackArea> blackAreas;

  /* Vector containing the positions of bad pixels */
  /* Format is x | (y << 16), so maximum pixel position is 65535 */
  // Positions of zeroes that must be interpolated
  std::vector<uint32> mBadPixelPositions GUARDED_BY(mBadPixelMutex);
  uchar8* mBadPixelMap = nullptr;
  uint32 mBadPixelMapPitch = 0;
  bool mDitherScale =
      true; // Should upscaling be done with dither to minimize banding?
  ImageMetaData metadata;

  Mutex mBadPixelMutex; // Mutex for 'mBadPixelPositions, must be used if more
                        // than 1 thread is accessing vector

private:
  uint32 dataRefCount GUARDED_BY(mymutex) = 0;

protected:
  RawImageType dataType;
  RawImageData();
  RawImageData(const iPoint2D &dim, uint32 bpp, uint32 cpp = 1);
  virtual void scaleValues(int start_y, int end_y) = 0;
  virtual void doLookup(int start_y, int end_y) = 0;
  virtual void fixBadPixel( uint32 x, uint32 y, int component = 0) = 0;
  void fixBadPixelsThread(int start_y, int end_y);
  void startWorker(RawImageWorker::RawImageWorkerTask task, bool cropped );
  uchar8* data = nullptr;
  uint32 cpp = 1; // Components per pixel
  uint32 bpp = 0; // Bytes per pixel.
  friend class RawImage;
  iPoint2D mOffset;
  iPoint2D uncropped_dim;
  std::unique_ptr<TableLookUp> table;
  Mutex mymutex;
};

class RawImageDataU16 final : public RawImageData {
public:
  void scaleBlackWhite() override;
  void calculateBlackAreas() override;
  void setWithLookUp(ushort16 value, uchar8* dst, uint32* random) override;

protected:
  void scaleValues_plain(int start_y, int end_y);
#ifdef WITH_SSE2
  void scaleValues_SSE2(int start_y, int end_y);
#endif
  void scaleValues(int start_y, int end_y) override;
  void fixBadPixel(uint32 x, uint32 y, int component = 0) override;
  void doLookup(int start_y, int end_y) override;

  RawImageDataU16();
  explicit RawImageDataU16(const iPoint2D& dim_, uint32 cpp_ = 1);
  friend class RawImage;
};

class RawImageDataFloat final : public RawImageData {
public:
  void scaleBlackWhite() override;
  void calculateBlackAreas() override;
  void setWithLookUp(ushort16 value, uchar8 *dst, uint32 *random) override;

protected:
  void scaleValues(int start_y, int end_y) override;
  void fixBadPixel(uint32 x, uint32 y, int component = 0) override;
  [[noreturn]] void doLookup(int start_y, int end_y) override;
  RawImageDataFloat();
  explicit RawImageDataFloat(const iPoint2D& dim_, uint32 cpp_ = 1);
  friend class RawImage;
};

 class RawImage {
 public:
   static RawImage create(RawImageType type = TYPE_USHORT16);
   static RawImage create(const iPoint2D &dim,
                          RawImageType type = TYPE_USHORT16,
                          uint32 componentsPerPixel = 1);
   RawImageData* operator->() const { return p_; }
   RawImageData& operator*() const { return *p_; }
   explicit RawImage(RawImageData* p); // p must not be NULL
   ~RawImage();
   RawImage(const RawImage& p);
   RawImage& operator=(const RawImage& p) noexcept;
   RawImage& operator=(RawImage&& p) noexcept;

   RawImageData* get() { return p_; }
 private:
   RawImageData* p_;    // p_ is never NULL
 };

inline RawImage RawImage::create(RawImageType type)  {
  switch (type)
  {
    case TYPE_USHORT16:
      return RawImage(new RawImageDataU16());
    case TYPE_FLOAT32:
      return RawImage(new RawImageDataFloat());
    default:
      writeLog(DEBUG_PRIO_ERROR, "RawImage::create: Unknown Image type!");
      __builtin_unreachable();

  }
}

inline RawImage RawImage::create(const iPoint2D& dim, RawImageType type, uint32 componentsPerPixel) {
  switch (type) {
  case TYPE_USHORT16:
    return RawImage(new RawImageDataU16(dim, componentsPerPixel));
  case TYPE_FLOAT32:
    return RawImage(new RawImageDataFloat(dim, componentsPerPixel));
  default:
    writeLog(DEBUG_PRIO_ERROR, "RawImage::create: Unknown Image type!");
    __builtin_unreachable();
  }
}

// setWithLookUp will set a single pixel by using the lookup table if supplied,
// You must supply the destination where the value should be written, and a pointer to
// a value that will be used to store a random counter that can be reused between calls.
// this needs to be inline to speed up tight decompressor loops
inline void RawImageDataU16::setWithLookUp(ushort16 value, uchar8* dst, uint32* random) {
  auto* dest = reinterpret_cast<ushort16*>(dst);
  if (table == nullptr) {
    *dest = value;
    return;
  }
  if (table->dither) {
    auto* t = reinterpret_cast<const uint32*>(table->tables.data());
    uint32 lookup = t[value];
    uint32 base = lookup & 0xffff;
    uint32 delta = lookup >> 16;
    uint32 r = *random;

    uint32 pix = base + ((delta * (r&2047) + 1024) >> 12);
    *random = 15700 *(r & 65535) + (r >> 16);
    *dest = pix;
    return;
  }
  *dest = table->tables[value];
}

class RawImageCurveGuard final {
  RawImage* mRaw;
  const std::vector<ushort16>& curve;
  const bool uncorrectedRawValues;

public:
  RawImageCurveGuard(RawImage* raw, const std::vector<ushort16>& curve_,
                     bool uncorrectedRawValues_)
      : mRaw(raw), curve(curve_), uncorrectedRawValues(uncorrectedRawValues_) {
    if (uncorrectedRawValues)
      return;

    (*mRaw)->setTable(curve, true);
  }

  ~RawImageCurveGuard() {
    // Set the table, if it should be needed later.
    if (uncorrectedRawValues)
      (*mRaw)->setTable(curve, false);
    else
      (*mRaw)->setTable(nullptr);
  }
};

} // namespace rawspeed
