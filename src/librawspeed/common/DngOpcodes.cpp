/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2017 Roman Lebedev

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

#include "common/DngOpcodes.h"
#include "common/Common.h"                // for uint32_t, uint16_t, clampBits
#include "common/Mutex.h"                 // for MutexLocker
#include "common/Point.h"                 // for iRectangle2D, iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/ByteStream.h"                // for ByteStream
#include "io/Endianness.h"                // for Endianness, Endianness::big
#include "tiff/TiffEntry.h"               // for TiffEntry
#include <algorithm>                      // for generate_n, fill_n
#include <cassert>                        // for assert
#include <cmath>                          // for pow
#include <iterator>                       // for back_insert_iterator
#include <limits>                         // for numeric_limits
#include <stdexcept>                      // for out_of_range
#include <tuple>                          // for tie, tuple
// IWYU pragma: no_include <ext/alloc_traits.h>
// IWYU pragma: no_include <type_traits>

#ifndef NDEBUG
#include <exception>
#endif

using std::vector;
using std::fill_n;
using std::make_pair;

namespace rawspeed {

namespace {

template <typename T>
iRectangle2D getImageCropAsRectangle(const CroppedArray2DRef<T>& img) {
  return {{img.offsetCols, img.offsetRows},
          {img.croppedWidth, img.croppedHeight}};
}

iRectangle2D getImageCropAsRectangle(const RawImage& ri) {
  switch (ri->getDataType()) {
  case RawImageType::UINT16:
    return getImageCropAsRectangle(ri->getU16DataAsCroppedArray2DRef());
  case RawImageType::F32:
    return getImageCropAsRectangle(ri->getF32DataAsCroppedArray2DRef());
  }
  __builtin_unreachable();
}

// FIXME: extract into `RawImage`?
template <typename T>
CroppedArray2DRef<T> getDataAsCroppedArray2DRef(const RawImage& ri) {
  if constexpr (std::is_same<T, uint16_t>())
    return ri->getU16DataAsCroppedArray2DRef();
  if constexpr (std::is_same<T, float>())
    return ri->getF32DataAsCroppedArray2DRef();
  __builtin_unreachable();
}

} // namespace

class DngOpcodes::DngOpcode {
#ifndef NDEBUG
  const iRectangle2D integrated_subimg;
  bool setup_was_called = false;
#endif

public:
  DngOpcode(const iRectangle2D& integrated_subimg_)
#ifndef NDEBUG
      : integrated_subimg(integrated_subimg_)
#endif
  {
    assert(std::uncaught_exceptions() == 0 &&
           "Creating DngOpcode during call stack unwinding?");
  }

  virtual ~DngOpcode() {
    assert((std::uncaught_exceptions() > 0 || setup_was_called) &&
           "Derived classes did not call our setup()!");
  }

  // Will be called once before processing.
  // Can be used for preparing pre-calculated values, etc.
  virtual void setup(const RawImage& ri) {
#ifndef NDEBUG
    setup_was_called = true;
#endif
    assert(integrated_subimg == getImageCropAsRectangle(ri) &&
           "Current image sub-crop does not match the expected one!");

    // NOP by default. child class shall override this if needed.
  }

  // Will be called for actual processing.
  virtual void apply(const RawImage& ri) = 0;
};

// ****************************************************************************

class DngOpcodes::FixBadPixelsConstant final : public DngOpcodes::DngOpcode {
  uint32_t value;

public:
  explicit FixBadPixelsConstant(const RawImage& ri, ByteStream& bs,
                                iRectangle2D& integrated_subimg_)
      : DngOpcodes::DngOpcode(integrated_subimg_), value(bs.getU32()) {
    bs.getU32(); // Bayer Phase not used
  }

  void setup(const RawImage& ri) override {
    DngOpcodes::DngOpcode::setup(ri);

    // These limitations are present within the DNG SDK as well.
    if (ri->getDataType() != RawImageType::UINT16)
      ThrowRDE("Only 16 bit images supported");

    if (ri->getCpp() > 1)
      ThrowRDE("Only 1 component images supported");
  }

  void apply(const RawImage& ri) override {
    MutexLocker guard(&ri->mBadPixelMutex);
    const CroppedArray2DRef<uint16_t> img(ri->getU16DataAsCroppedArray2DRef());
    iPoint2D crop = ri->getCropOffset();
    uint32_t offset = crop.x | (crop.y << 16);
    for (auto row = 0; row < img.croppedHeight; ++row) {
      for (auto col = 0; col < img.croppedWidth; ++col) {
        if (img(row, col) == value)
          ri->mBadPixelPositions.push_back(offset + (row << 16 | col));
      }
    }
  }
};

// ****************************************************************************

class DngOpcodes::ROIOpcode : public DngOpcodes::DngOpcode {
  iRectangle2D roi;

protected:
  explicit ROIOpcode(const RawImage& ri, ByteStream& bs,
                     iRectangle2D& integrated_subimg_)
      : DngOpcodes::DngOpcode(integrated_subimg_) {
    const iRectangle2D subImage = iRectangle2D({0, 0}, integrated_subimg_.dim);

    uint32_t top = bs.getU32();
    uint32_t left = bs.getU32();
    uint32_t bottom = bs.getU32();
    uint32_t right = bs.getU32();

    const iPoint2D topLeft(left, top);
    const iPoint2D bottomRight(right, bottom);

    if (!(subImage.isPointInsideInclusive(topLeft) &&
          subImage.isPointInsideInclusive(bottomRight) &&
          bottomRight >= topLeft)) {
      ThrowRDE("Rectangle (%u, %u, %u, %u) not inside image (%u, %u, %u, %u).",
               topLeft.x, topLeft.y, bottomRight.x, bottomRight.y,
               subImage.getTopLeft().x, subImage.getTopLeft().y,
               subImage.getBottomRight().x, subImage.getBottomRight().y);
    }

    roi.setTopLeft(topLeft);
    roi.setBottomRightAbsolute(bottomRight);
    assert(roi.isThisInside(subImage));
  }

  [[nodiscard]] const iRectangle2D& __attribute__((pure)) getRoi() const {
    return roi;
  }
};

// ****************************************************************************

class DngOpcodes::DummyROIOpcode final : public ROIOpcode {
public:
  explicit DummyROIOpcode(const RawImage& ri, ByteStream& bs,
                          iRectangle2D& integrated_subimg_)
      : ROIOpcode(ri, bs, integrated_subimg_) {
    DummyROIOpcode::setup(ri);
  }

  [[nodiscard]] const iRectangle2D& __attribute__((pure)) getRoi() const {
    return ROIOpcode::getRoi();
  }

  [[noreturn]] void apply(const RawImage& ri) override {
    // NOLINTNEXTLINE: https://bugs.llvm.org/show_bug.cgi?id=50532
    assert(false && "You should not be calling this.");
    __builtin_unreachable();
  }
};

// ****************************************************************************

class DngOpcodes::FixBadPixelsList final : public DngOpcodes::DngOpcode {
  std::vector<uint32_t> badPixels;

public:
  explicit FixBadPixelsList(const RawImage& ri, ByteStream& bs,
                            iRectangle2D& integrated_subimg_)
      : DngOpcodes::DngOpcode(integrated_subimg_) {
    // Although it is not really obvious from the spec,
    // the coordinates appear to be global/crop-independent,
    // and apply to the source uncropped image.
    const iRectangle2D fullImage(0, 0, ri->getUncroppedDim().x - 1,
                                 ri->getUncroppedDim().y - 1);

    bs.getU32(); // Skip phase - we don't care
    auto badPointCount = bs.getU32();
    auto badRectCount = bs.getU32();

    // first, check that we indeed have much enough data
    const auto origPos = bs.getPosition();
    bs.skipBytes(badPointCount, 2 * 4);
    bs.skipBytes(badRectCount, 4 * 4);
    bs.setPosition(origPos);

    // Read points
    badPixels.reserve(badPixels.size() + badPointCount);
    for (auto i = 0U; i < badPointCount; ++i) {
      auto y = bs.getU32();
      auto x = bs.getU32();

      if (const iPoint2D badPoint(x, y);
          !fullImage.isPointInsideInclusive(badPoint))
        ThrowRDE("Bad point not inside image.");

      badPixels.emplace_back(y << 16 | x);
    }

    // Read rects
    for (auto i = 0U; i < badRectCount; ++i) {
      const DummyROIOpcode dummy(ri, bs, integrated_subimg_);

      const iRectangle2D badRect = dummy.getRoi();
      assert(badRect.isThisInside(fullImage));

      auto area = (1 + badRect.getHeight()) * (1 + badRect.getWidth());
      badPixels.reserve(badPixels.size() + area);
      for (auto y = badRect.getTop(); y <= badRect.getBottom(); ++y) {
        for (auto x = badRect.getLeft(); x <= badRect.getRight(); ++x) {
          badPixels.emplace_back(y << 16 | x);
        }
      }
    }
  }

  void apply(const RawImage& ri) override {
    MutexLocker guard(&ri->mBadPixelMutex);
    ri->mBadPixelPositions.insert(ri->mBadPixelPositions.begin(),
                                  badPixels.begin(), badPixels.end());
  }
};

// ****************************************************************************

class DngOpcodes::TrimBounds final : public ROIOpcode {
public:
  explicit TrimBounds(const RawImage& ri, ByteStream& bs,
                      iRectangle2D& integrated_subimg_)
      : ROIOpcode(ri, bs, integrated_subimg_) {
    integrated_subimg_ = getRoi();
  }

  void apply(const RawImage& ri) override { ri->subFrame(getRoi()); }
};

// ****************************************************************************

class DngOpcodes::PixelOpcode : public ROIOpcode {
  uint32_t firstPlane;
  uint32_t planes;
  uint32_t rowPitch;
  uint32_t colPitch;

protected:
  explicit PixelOpcode(const RawImage& ri, ByteStream& bs,
                       iRectangle2D& integrated_subimg_)
      : ROIOpcode(ri, bs, integrated_subimg_), firstPlane(bs.getU32()),
        planes(bs.getU32()) {

    if (planes == 0 || firstPlane > ri->getCpp() || planes > ri->getCpp() ||
        firstPlane + planes > ri->getCpp()) {
      ThrowRDE("Bad plane params (first %u, num %u), got planes = %u",
               firstPlane, planes, ri->getCpp());
    }

    rowPitch = bs.getU32();
    colPitch = bs.getU32();

    const iRectangle2D& ROI = getRoi();

    if (rowPitch < 1 || rowPitch > static_cast<uint32_t>(ROI.getHeight()) ||
        colPitch < 1 || colPitch > static_cast<uint32_t>(ROI.getWidth()))
      ThrowRDE("Invalid pitch");
  }

  [[nodiscard]] const iPoint2D __attribute__((pure)) getPitch() const {
    return {static_cast<int>(colPitch), static_cast<int>(rowPitch)};
  }

  // traverses the current ROI and applies the operation OP to each pixel,
  // i.e. each pixel value v is replaced by op(x, y, v), where x/y are the
  // coordinates of the pixel value v.
  template <typename T, typename OP> void applyOP(const RawImage& ri, OP op) {
    const CroppedArray2DRef<T> img = getDataAsCroppedArray2DRef<T>(ri);
    int cpp = ri->getCpp();
    const iRectangle2D& ROI = getRoi();
    const iPoint2D numAffected(roundUpDivision(getRoi().dim.x, colPitch),
                               roundUpDivision(getRoi().dim.y, rowPitch));
    for (int y = 0; y < numAffected.y; ++y) {
      for (int x = 0; x < numAffected.x; ++x) {
        for (auto p = 0U; p < planes; ++p) {
          T& pixel = img(ROI.getTop() + rowPitch * y,
                         firstPlane + (ROI.getLeft() + colPitch * x) * cpp + p);
          pixel = op(x, y, pixel);
        }
      }
    }
  }
};

// ****************************************************************************

class DngOpcodes::LookupOpcode : public PixelOpcode {
protected:
  vector<uint16_t> lookup;

  explicit LookupOpcode(const RawImage& ri, ByteStream& bs,
                        iRectangle2D& integrated_subimg_)
      : PixelOpcode(ri, bs, integrated_subimg_), lookup(65536) {}

  void setup(const RawImage& ri) override {
    PixelOpcode::setup(ri);

    if (ri->getDataType() != RawImageType::UINT16)
      ThrowRDE("Only 16 bit images supported");
  }

  void apply(const RawImage& ri) override {
    applyOP<uint16_t>(ri, [this]([[maybe_unused]] uint32_t x,
                                 [[maybe_unused]] uint32_t y,
                                 uint16_t v) { return lookup[v]; });
  }
};

// ****************************************************************************

class DngOpcodes::TableMap final : public LookupOpcode {
public:
  explicit TableMap(const RawImage& ri, ByteStream& bs,
                    iRectangle2D& integrated_subimg_)
      : LookupOpcode(ri, bs, integrated_subimg_) {
    auto count = bs.getU32();

    if (count == 0 || count > 65536)
      ThrowRDE("Invalid size of lookup table");

    for (auto i = 0U; i < count; ++i)
      lookup[i] = bs.getU16();

    if (count < lookup.size())
      fill_n(&lookup[count], lookup.size() - count, lookup[count - 1]);
  }
};

// ****************************************************************************

class DngOpcodes::PolynomialMap final : public LookupOpcode {
public:
  explicit PolynomialMap(const RawImage& ri, ByteStream& bs,
                         iRectangle2D& integrated_subimg_)
      : LookupOpcode(ri, bs, integrated_subimg_) {
    vector<double> polynomial;

    const auto polynomial_size = bs.getU32() + 1UL;
    (void)bs.check(8UL * polynomial_size);
    if (polynomial_size > 9)
      ThrowRDE("A polynomial with more than 8 degrees not allowed");

    polynomial.reserve(polynomial_size);
    std::generate_n(std::back_inserter(polynomial), polynomial_size,
                    [&bs]() { return bs.get<double>(); });

    // Create lookup
    lookup.resize(65536);
    for (auto i = 0UL; i < lookup.size(); ++i) {
      double val = polynomial[0];
      for (auto j = 1UL; j < polynomial.size(); ++j)
        val += polynomial[j] * pow(i / 65536.0, j);
      lookup[i] = (clampBits(static_cast<int>(val * 65535.5), 16));
    }
  }
};

// ****************************************************************************

class DngOpcodes::DeltaRowOrColBase : public PixelOpcode {
public:
  struct SelectX {
    static inline uint32_t select(uint32_t x, uint32_t /*y*/) { return x; }
  };

  struct SelectY {
    static inline uint32_t select(uint32_t /*x*/, uint32_t y) { return y; }
  };

protected:
  DeltaRowOrColBase(const RawImage& ri, ByteStream& bs,
                    iRectangle2D& integrated_subimg_)
      : PixelOpcode(ri, bs, integrated_subimg_) {}
};

template <typename S>
class DngOpcodes::DeltaRowOrCol : public DeltaRowOrColBase {
public:
  void setup(const RawImage& ri) override {
    PixelOpcode::setup(ri);

    // If we are working on a float image, no need to convert to int
    if (ri->getDataType() != RawImageType::UINT16)
      return;

    deltaI.reserve(deltaF.size());
    for (const auto f : deltaF) {
      if (!valueIsOk(f))
        ThrowRDE("Got float %f which is unacceptable.", f);
      deltaI.emplace_back(static_cast<int>(f2iScale * f));
    }
  }

protected:
  const float f2iScale;
  vector<float> deltaF;
  vector<int> deltaI;

  // only meaningful for uint16_t images!
  virtual bool valueIsOk(float value) = 0;

  DeltaRowOrCol(const RawImage& ri, ByteStream& bs,
                iRectangle2D& integrated_subimg_, float f2iScale_)
      : DeltaRowOrColBase(ri, bs, integrated_subimg_), f2iScale(f2iScale_) {
    const auto deltaF_count = bs.getU32();
    (void)bs.check(deltaF_count, 4);

    // See PixelOpcode::applyOP(). We will access deltaF/deltaI up to (excl.)
    // either ROI.getWidth() or ROI.getHeight() index. Thus, we need to have
    // either ROI.getRight() or ROI.getBottom() elements in there.
    if (const auto expectedSize = roundUpDivision(
            S::select(getRoi().getWidth(), getRoi().getHeight()),
            S::select(getPitch().x, getPitch().y));
        expectedSize != deltaF_count) {
      ThrowRDE("Got unexpected number of elements (%lu), expected %u.",
               expectedSize, deltaF_count);
    }

    deltaF.reserve(deltaF_count);
    std::generate_n(std::back_inserter(deltaF), deltaF_count, [&bs]() {
      const auto F = bs.get<float>();
      if (!std::isfinite(F))
        ThrowRDE("Got bad float %f.", F);
      return F;
    });
  }
};

// ****************************************************************************

template <typename S>
class DngOpcodes::OffsetPerRowOrCol final : public DeltaRowOrCol<S> {
  // We have pixel value in range of [0..65535]. We apply some offset X.
  // For this to generate a value within the same range , the offset X needs
  // to have an absolute value of 65535. Since the offset is multiplied
  // by f2iScale before applying, we need to divide by f2iScale here.
  const double absLimit;

  bool valueIsOk(float value) override { return std::abs(value) <= absLimit; }

public:
  explicit OffsetPerRowOrCol(const RawImage& ri, ByteStream& bs,
                             iRectangle2D& integrated_subimg_)
      : DeltaRowOrCol<S>(ri, bs, integrated_subimg_, 65535.0F),
        absLimit(double(std::numeric_limits<uint16_t>::max()) /
                 this->f2iScale) {}

  void apply(const RawImage& ri) override {
    if (ri->getDataType() == RawImageType::UINT16) {
      this->template applyOP<uint16_t>(
          ri, [this](uint32_t x, uint32_t y, uint16_t v) {
            return clampBits(this->deltaI[S::select(x, y)] + v, 16);
          });
    } else {
      this->template applyOP<float>(ri,
                                    [this](uint32_t x, uint32_t y, float v) {
                                      return this->deltaF[S::select(x, y)] + v;
                                    });
    }
  }
};

template <typename S>
class DngOpcodes::ScalePerRowOrCol final : public DeltaRowOrCol<S> {
  // We have pixel value in range of [0..65535]. We scale by float X.
  // For this to generate a value within the same range, the scale X needs
  // to be in the range [0..65535]. However, we are operating with 32-bit
  // signed integer space, so the new value can not be larger than 2^31,
  // else we'd have signed integer overflow. Since the offset is multiplied
  // by f2iScale before applying, we need to divide by f2iScale here.
  static constexpr const double minLimit = 0.0;
  static constexpr int rounding = 512;
  const double maxLimit;

  bool valueIsOk(float value) override {
    return value >= minLimit && value <= maxLimit;
  }

public:
  explicit ScalePerRowOrCol(const RawImage& ri, ByteStream& bs,
                            iRectangle2D& integrated_subimg_)
      : DeltaRowOrCol<S>(ri, bs, integrated_subimg_, 1024.0F),
        maxLimit((double(std::numeric_limits<int>::max() - rounding) /
                  double(std::numeric_limits<uint16_t>::max())) /
                 this->f2iScale) {}

  void apply(const RawImage& ri) override {
    if (ri->getDataType() == RawImageType::UINT16) {
      this->template applyOP<uint16_t>(ri, [this](uint32_t x, uint32_t y,
                                                  uint16_t v) {
        return clampBits((this->deltaI[S::select(x, y)] * v + 512) >> 10, 16);
      });
    } else {
      this->template applyOP<float>(ri,
                                    [this](uint32_t x, uint32_t y, float v) {
                                      return this->deltaF[S::select(x, y)] * v;
                                    });
    }
  }
};

// ****************************************************************************

DngOpcodes::DngOpcodes(const RawImage& ri, ByteStream bs) {
  // DNG opcodes are always stored in big-endian byte order.
  bs.setByteOrder(Endianness::big);

  const auto opcode_count = bs.getU32();
  auto origPos = bs.getPosition();

  // validate opcode count. we either have to do this, or we can't preallocate
  for (auto i = 0U; i < opcode_count; i++) {
    bs.skipBytes(4); // code
    bs.skipBytes(4); // version
    bs.skipBytes(4); // flags
    const auto opcode_size = bs.getU32();
    bs.skipBytes(opcode_size);
  }

  bs.setPosition(origPos);

  // okay, we may indeed have that many opcodes in here. now let's reserve
  opcodes.reserve(opcode_count);

  iRectangle2D integrated_subimg = getImageCropAsRectangle(ri);

  for (auto i = 0U; i < opcode_count; i++) {
    auto code = bs.getU32();
    bs.skipBytes(4); // ignore version
#ifdef DEBUG
    bs.skipBytes(4); // ignore flags
#else
    auto flags = bs.getU32();
#endif
    const auto opcode_size = bs.getU32();
    ByteStream opcode_bs = bs.getStream(opcode_size);

    const char* opName = nullptr;
    constructor_t opConstructor = nullptr;
    try {
      std::tie(opName, opConstructor) = Map.at(code);
    } catch (const std::out_of_range&) {
      ThrowRDE("Unknown unhandled Opcode: %d", code);
    }

    if (opConstructor != nullptr)
      opcodes.emplace_back(opConstructor(ri, opcode_bs, integrated_subimg));
    else {
#ifndef DEBUG
      // Throw Error if not marked as optional
      if (!(flags & 1))
#endif
        ThrowRDE("Unsupported Opcode: %d (%s)", code, opName);
    }

    if (opcode_bs.getRemainSize() != 0)
      ThrowRDE("Inconsistent length of opcode");
  }

#ifdef DEBUG
  assert(opcodes.size() == opcode_count);
#endif
}

// Defined here as empty destructor, otherwise we'd need a complete definition
// of the DngOpcode type in DngOpcodes.h
DngOpcodes::~DngOpcodes() = default;

void DngOpcodes::applyOpCodes(const RawImage& ri) const {
  for (const auto& code : opcodes) {
    code->setup(ri);
    code->apply(ri);
  }
}

template <class Opcode>
std::unique_ptr<DngOpcodes::DngOpcode>
DngOpcodes::constructor(const RawImage& ri, ByteStream& bs,
                        iRectangle2D& integrated_subimg) {
  return std::make_unique<Opcode>(ri, bs, integrated_subimg);
}

// ALL opcodes specified in DNG Specification MUST be listed here.
// however, some of them might not be implemented.
const std::map<uint32_t, std::pair<const char*, DngOpcodes::constructor_t>>
    DngOpcodes::Map = {
        {1U, make_pair("WarpRectilinear", nullptr)},
        {2U, make_pair("WarpFisheye", nullptr)},
        {3U, make_pair("FixVignetteRadial", nullptr)},
        {4U,
         make_pair("FixBadPixelsConstant",
                   &DngOpcodes::constructor<DngOpcodes::FixBadPixelsConstant>)},
        {5U, make_pair("FixBadPixelsList",
                       &DngOpcodes::constructor<DngOpcodes::FixBadPixelsList>)},
        {6U, make_pair("TrimBounds",
                       &DngOpcodes::constructor<DngOpcodes::TrimBounds>)},
        {7U,
         make_pair("MapTable", &DngOpcodes::constructor<DngOpcodes::TableMap>)},
        {8U, make_pair("MapPolynomial",
                       &DngOpcodes::constructor<DngOpcodes::PolynomialMap>)},
        {9U, make_pair("GainMap", nullptr)},
        {10U,
         make_pair(
             "DeltaPerRow",
             &DngOpcodes::constructor<
                 DngOpcodes::OffsetPerRowOrCol<DeltaRowOrColBase::SelectY>>)},
        {11U,
         make_pair(
             "DeltaPerColumn",
             &DngOpcodes::constructor<
                 DngOpcodes::OffsetPerRowOrCol<DeltaRowOrColBase::SelectX>>)},
        {12U,
         make_pair(
             "ScalePerRow",
             &DngOpcodes::constructor<
                 DngOpcodes::ScalePerRowOrCol<DeltaRowOrColBase::SelectY>>)},
        {13U,
         make_pair(
             "ScalePerColumn",
             &DngOpcodes::constructor<
                 DngOpcodes::ScalePerRowOrCol<DeltaRowOrColBase::SelectX>>)},

};

} // namespace rawspeed
