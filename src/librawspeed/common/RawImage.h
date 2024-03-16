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
#include "ThreadSafetyAnalysis.h"
#include "adt/AlignedAllocator.h"
#include "adt/Array2DRef.h"
#include "adt/Casts.h"
#include "adt/CroppedArray2DRef.h"
#include "adt/DefaultInitAllocatorAdaptor.h"
#include "adt/Mutex.h"
#include "adt/NotARational.h"
#include "adt/Optional.h"
#include "adt/Point.h"
#include "common/Common.h"
#include "common/ErrorLog.h"
#include "common/TableLookUp.h"
#include "metadata/BlackArea.h"
#include "metadata/ColorFilterArray.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rawspeed {

class RawImage;
class RawImageData;

enum class RawImageType : uint8_t { UINT16, F32 };

class RawImageWorker final {
public:
  enum class RawImageWorkerTask : uint16_t {
    SCALE_VALUES = 1,
    FIX_BAD_PIXELS = 2,
    APPLY_LOOKUP = 3 | 0x1000,
    FULL_IMAGE = 0x1000
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

class ImageMetaData final {
public:
  // Aspect ratio of the pixels, usually 1 but some cameras need scaling
  // <1 means the image needs to be stretched vertically, (0.5 means 2x)
  // >1 means the image needs to be stretched horizontally (2 mean 2x)
  double pixelAspectRatio = 1;

  // White balance coefficients of the image
  std::array<float, 4> wbCoeffs = {{NAN, NAN, NAN, NAN}};

  // If not empty, a row-major color matrix,
  // that converts XYZ values to reference camera native color space values,
  // under calibration illuminant 21 (D65).
  std::vector<NotARational<int>> colorMatrix;

  // How many pixels far down the left edge and far up the right edge the image
  // corners are when the image is rotated 45 degrees in Fuji rotated sensors.
  uint32_t fujiRotationPos = 0;

  iPoint2D subsampling = {1, 1};
  std::string make;
  std::string model;
  std::string mode;

  std::string canonical_make;
  std::string canonical_model;
  std::string canonical_alias;
  std::string canonical_id;

  // ISO speed. If known the value is set, otherwise it will be '0'.
  int isoSpeed = 0;
};

class RawImageData : public ErrorLog {
  virtual void anchor() const;

  friend class RawImageWorker;

public:
  virtual ~RawImageData() = default;
  [[nodiscard]] uint32_t RAWSPEED_READONLY getCpp() const { return cpp; }
  [[nodiscard]] uint32_t RAWSPEED_READONLY getBpp() const { return bpp; }
  void setCpp(uint32_t val);
  void createData();
  void poisonPadding();
  void unpoisonPadding();

  [[nodiscard]] rawspeed::RawImageType getDataType() const { return dataType; }

  [[nodiscard]] Array2DRef<uint16_t> getU16DataAsUncroppedArray2DRef() noexcept;
  [[nodiscard]] CroppedArray2DRef<uint16_t>
  getU16DataAsCroppedArray2DRef() noexcept;
  [[nodiscard]] Array2DRef<float> getF32DataAsUncroppedArray2DRef() noexcept;
  [[nodiscard]] CroppedArray2DRef<float>
  getF32DataAsCroppedArray2DRef() noexcept;

  // WARNING: this is most certainly not what you want!
  [[nodiscard]] Array2DRef<std::byte>
  getByteDataAsUncroppedArray2DRef() noexcept;

  void subFrame(iRectangle2D cropped);
  void clearArea(iRectangle2D area);
  [[nodiscard]] iPoint2D RAWSPEED_READONLY getUncroppedDim() const;
  [[nodiscard]] iPoint2D RAWSPEED_READONLY getCropOffset() const;
  virtual void scaleBlackWhite() = 0;
  virtual void calculateBlackAreas() = 0;
  virtual void setWithLookUp(uint16_t value, std::byte* dst,
                             uint32_t* random) = 0;
  void sixteenBitLookup();
  void transferBadPixelsToMap() REQUIRES(!mBadPixelMutex);
  void fixBadPixels() REQUIRES(!mBadPixelMutex);
  void setTable(const std::vector<uint16_t>& table_, bool dither);
  void setTable(std::unique_ptr<TableLookUp> t);

  [[nodiscard]] bool isAllocated() const { return !data.empty(); }
  void createBadPixelMap();
  iPoint2D dim;
  int pitch = 0;

  // padding is the size of the area after last pixel of line n
  // and before the first pixel of line n+1
  uint32_t padding = 0;

  bool isCFA{true};
  ColorFilterArray cfa;
  int blackLevel = -1;
  std::array<int, 4> blackLevelSeparateStorage;
  Optional<Array2DRef<int>> blackLevelSeparate;

  // A white level of the image, if known.
  // NOTE: it is always correct to divide the pixel by `float(whiteLevel)`,
  //       to normalize the image.
  // NOTE: for floating-point images, the white level is never non-integral,
  //       and thus >= 1.0f
  Optional<int> whitePoint;

  std::vector<BlackArea> blackAreas;

  /* Vector containing the positions of bad pixels */
  /* Format is x | (y << 16), so maximum pixel position is 65535 */
  // Positions of zeroes that must be interpolated
  std::vector<uint32_t> mBadPixelPositions GUARDED_BY(mBadPixelMutex);
  std::vector<uint8_t, AlignedAllocator<uint8_t, 16>> mBadPixelMap;
  uint32_t mBadPixelMapPitch = 0;
  bool mDitherScale =
      true; // Should upscaling be done with dither to minimize banding?
  ImageMetaData metadata;

  Mutex mBadPixelMutex; // Mutex for 'mBadPixelPositions, must be used if more
                        // than 1 thread is accessing vector

protected:
  RawImageType dataType;
  RawImageData() = default;
  RawImageData(RawImageType type, const iPoint2D& dim, int bpp, int cpp = 1);
  virtual void scaleValues(int start_y, int end_y) = 0;
  virtual void doLookup(int start_y, int end_y) = 0;
  virtual void fixBadPixel(uint32_t x, uint32_t y, int component = 0) = 0;
  void fixBadPixelsThread(int start_y, int end_y);
  void startWorker(RawImageWorker::RawImageWorkerTask task, bool cropped);
  std::vector<uint8_t, DefaultInitAllocatorAdaptor<
                           uint8_t, AlignedAllocator<uint8_t, 16>>>
      data;
  int cpp = 1; // Components per pixel
  int bpp = 0; // Bytes per pixel.
  friend class RawImage;
  iPoint2D mOffset;
  iPoint2D uncropped_dim;
  std::unique_ptr<TableLookUp> table;
};

class RawImageDataU16 final : public RawImageData {
public:
  RawImageDataU16();
  explicit RawImageDataU16(const iPoint2D& dim_, uint32_t cpp_ = 1);

  void scaleBlackWhite() override;
  void calculateBlackAreas() override;
  void setWithLookUp(uint16_t value, std::byte* dst, uint32_t* random) override;

private:
  void scaleValues_plain(int start_y, int end_y);
#ifdef WITH_SSE2
  void scaleValues_SSE2(int start_y, int end_y);
#endif
  void scaleValues(int start_y, int end_y) override;
  void fixBadPixel(uint32_t x, uint32_t y, int component = 0) override;
  void doLookup(int start_y, int end_y) override;

  friend class RawImage;
};

class RawImageDataFloat final : public RawImageData {
public:
  RawImageDataFloat();
  explicit RawImageDataFloat(const iPoint2D& dim_, uint32_t cpp_ = 1);

  void scaleBlackWhite() override;
  void calculateBlackAreas() override;
  void setWithLookUp(uint16_t value, std::byte* dst, uint32_t* random) override;

private:
  void scaleValues(int start_y, int end_y) override;
  void fixBadPixel(uint32_t x, uint32_t y, int component = 0) override;
  [[noreturn]] void doLookup(int start_y, int end_y) override;

  friend class RawImage;
};

class RawImage final {
public:
  static RawImage create(RawImageType type = RawImageType::UINT16);
  static RawImage create(const iPoint2D& dim,
                         RawImageType type = RawImageType::UINT16,
                         uint32_t componentsPerPixel = 1);
  RawImageData* RAWSPEED_READONLY operator->() const { return &*p_; }
  RawImageData& RAWSPEED_READONLY operator*() const { return *p_; }

  explicit RawImage(RawImageData* p) = delete;
  explicit RawImage(std::shared_ptr<RawImageData> p) : p_(std::move(p)) {}

  RawImageData* get() { return &*p_; }

private:
  std::shared_ptr<RawImageData> p_; // p_ is never NULL
};

inline RawImage RawImage::create(RawImageType type) {
  switch (type) {
  case RawImageType::UINT16:
    return RawImage(std::make_shared<RawImageDataU16>());
  case RawImageType::F32:
    return RawImage(std::make_shared<RawImageDataFloat>());
  }
  writeLog(DEBUG_PRIO::ERROR, "RawImage::create: Unknown Image type!");
  __builtin_unreachable();
}

inline RawImage RawImage::create(const iPoint2D& dim, RawImageType type,
                                 uint32_t componentsPerPixel) {
  switch (type) {
  case RawImageType::UINT16:
    return RawImage(std::make_shared<RawImageDataU16>(dim, componentsPerPixel));
  case RawImageType::F32:
    return RawImage(
        std::make_shared<RawImageDataFloat>(dim, componentsPerPixel));
  }
  writeLog(DEBUG_PRIO::ERROR, "RawImage::create: Unknown Image type!");
  __builtin_unreachable();
}

inline Array2DRef<uint16_t>
RawImageData::getU16DataAsUncroppedArray2DRef() noexcept {
  assert(dataType == RawImageType::UINT16 &&
         "Attempting to access floating-point buffer as uint16_t.");
  assert(!data.empty() && "Data not yet allocated.");
  return {reinterpret_cast<uint16_t*>(data.data()), cpp * uncropped_dim.x,
          uncropped_dim.y, static_cast<int>(pitch / sizeof(uint16_t))};
}

inline CroppedArray2DRef<uint16_t>
RawImageData::getU16DataAsCroppedArray2DRef() noexcept {
  return {getU16DataAsUncroppedArray2DRef(), cpp * mOffset.x, mOffset.y,
          cpp * dim.x, dim.y};
}

inline Array2DRef<float>
RawImageData::getF32DataAsUncroppedArray2DRef() noexcept {
  assert(dataType == RawImageType::F32 &&
         "Attempting to access integer buffer as float.");
  assert(!data.empty() && "Data not yet allocated.");
  return {reinterpret_cast<float*>(data.data()), cpp * uncropped_dim.x,
          uncropped_dim.y, static_cast<int>(pitch / sizeof(float))};
}

inline CroppedArray2DRef<float>
RawImageData::getF32DataAsCroppedArray2DRef() noexcept {
  return {getF32DataAsUncroppedArray2DRef(), cpp * mOffset.x, mOffset.y,
          cpp * dim.x, dim.y};
}

inline Array2DRef<std::byte>
RawImageData::getByteDataAsUncroppedArray2DRef() noexcept {
  switch (dataType) {
  case RawImageType::UINT16:
    return getU16DataAsUncroppedArray2DRef();
  case RawImageType::F32:
    return getF32DataAsUncroppedArray2DRef();
  }
  __builtin_unreachable();
}

// setWithLookUp will set a single pixel by using the lookup table if supplied,
// You must supply the destination where the value should be written, and a
// pointer to a value that will be used to store a random counter that can be
// reused between calls. this needs to be inline to speed up tight decompressor
// loops
inline void RawImageDataU16::setWithLookUp(uint16_t value, std::byte* dst,
                                           uint32_t* random) {
  auto* dest = reinterpret_cast<uint16_t*>(dst);
  if (table == nullptr) {
    *dest = value;
    return;
  }
  if (table->dither) {
    uint32_t base = table->tables[2 * value + 0];
    uint32_t delta = table->tables[2 * value + 1];
    uint32_t r = *random;

    uint32_t pix = base + ((delta * (r & 2047) + 1024) >> 12);
    *random = 15700 * (r & 65535) + (r >> 16);
    *dest = implicit_cast<uint16_t>(pix);
    return;
  }
  *dest = table->tables[value];
}

class RawImageCurveGuard final {
  const RawImage* mRaw;
  const std::vector<uint16_t>& curve;
  const bool uncorrectedRawValues;

public:
  RawImageCurveGuard() = delete;
  RawImageCurveGuard(const RawImageCurveGuard&) = delete;
  RawImageCurveGuard(RawImageCurveGuard&&) noexcept = delete;
  RawImageCurveGuard& operator=(const RawImageCurveGuard&) noexcept = delete;
  RawImageCurveGuard& operator=(RawImageCurveGuard&&) noexcept = delete;

  RawImageCurveGuard(const RawImage* raw, const std::vector<uint16_t>& curve_,
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
