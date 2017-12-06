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
#include "common/Common.h"                // for uint32, ushort16, clampBits
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

using std::vector;
using std::fill_n;
using std::make_pair;

namespace rawspeed {

class DngOpcodes::DngOpcode {
public:
  virtual ~DngOpcode() = default;

  // Will be called once before processing.
  // Can be used for preparing pre-calculated values, etc.
  virtual void setup(const RawImage& ri) {
    // NOP by default. child class shall override this if needed.
  }

  // Will be called for actual processing.
  virtual void apply(const RawImage& ri) = 0;
};

// ****************************************************************************

class DngOpcodes::FixBadPixelsConstant final : public DngOpcodes::DngOpcode {
  uint32 value;

public:
  explicit FixBadPixelsConstant(const RawImage& ri, ByteStream* bs) {
    value = bs->getU32();
    bs->getU32(); // Bayer Phase not used
  }

  void setup(const RawImage& ri) override {
    // These limitations are present within the DNG SDK as well.
    if (ri->getDataType() != TYPE_USHORT16)
      ThrowRDE("Only 16 bit images supported");

    if (ri->getCpp() > 1)
      ThrowRDE("Only 1 component images supported");
  }

  void apply(const RawImage& ri) override {
    MutexLocker guard(&ri->mBadPixelMutex);
    iPoint2D crop = ri->getCropOffset();
    uint32 offset = crop.x | (crop.y << 16);
    for (auto y = 0; y < ri->dim.y; ++y) {
      auto* src = reinterpret_cast<ushort16*>(ri->getData(0, y));
      for (auto x = 0; x < ri->dim.x; ++x) {
        if (src[x] == value)
          ri->mBadPixelPositions.push_back(offset + (y << 16 | x));
      }
    }
  }
};

// ****************************************************************************

class DngOpcodes::ROIOpcode : public DngOpcodes::DngOpcode {
  iRectangle2D roi;

protected:
  explicit ROIOpcode(const RawImage& ri, ByteStream* bs, bool minusOne) {
    const iRectangle2D fullImage =
        minusOne ? iRectangle2D(0, 0, ri->dim.x - 1, ri->dim.y - 1)
                 : iRectangle2D(0, 0, ri->dim.x, ri->dim.y);

    uint32 top = bs->getU32();
    uint32 left = bs->getU32();
    uint32 bottom = bs->getU32();
    uint32 right = bs->getU32();

    const iPoint2D topLeft(left, top);
    const iPoint2D bottomRight(right, bottom);

    if (!(fullImage.isPointInside(topLeft) &&
          fullImage.isPointInside(bottomRight) && bottomRight >= topLeft)) {
      ThrowRDE("Rectangle (%u, %u, %u, %u) not inside image (%u, %u, %u, %u).",
               topLeft.x, topLeft.y, bottomRight.x, bottomRight.y,
               fullImage.getTopLeft().x, fullImage.getTopLeft().y,
               fullImage.getBottomRight().x, fullImage.getBottomRight().y);
    }

    roi.setTopLeft(topLeft);
    roi.setBottomRightAbsolute(bottomRight);
    assert(roi.isThisInside(fullImage));
  }

  const iRectangle2D& __attribute__((pure)) getRoi() const { return roi; }
};

// ****************************************************************************

class DngOpcodes::DummyROIOpcode final : public ROIOpcode {
public:
  explicit DummyROIOpcode(const RawImage& ri, ByteStream* bs)
      : ROIOpcode(ri, bs, true) {}

  const iRectangle2D& __attribute__((pure)) getRoi() const {
    return ROIOpcode::getRoi();
  }

  [[noreturn]] void apply(const RawImage& ri) final {
    assert(false && "You should not be calling this.");
    __builtin_unreachable();
  }
};

// ****************************************************************************

class DngOpcodes::FixBadPixelsList final : public DngOpcodes::DngOpcode {
  std::vector<uint32> badPixels;

public:
  explicit FixBadPixelsList(const RawImage& ri, ByteStream* bs) {
    const iRectangle2D fullImage(0, 0, ri->getUncroppedDim().x - 1,
                                 ri->getUncroppedDim().y - 1);

    bs->getU32(); // Skip phase - we don't care
    auto badPointCount = bs->getU32();
    auto badRectCount = bs->getU32();

    // first, check that we indeed have much enough data
    const auto origPos = bs->getPosition();
    bs->skipBytes(badPointCount, 2 * 4);
    bs->skipBytes(badRectCount, 4 * 4);
    bs->setPosition(origPos);

    // Read points
    badPixels.reserve(badPixels.size() + badPointCount);
    for (auto i = 0U; i < badPointCount; ++i) {
      auto y = bs->getU32();
      auto x = bs->getU32();

      const iPoint2D badPoint(x, y);
      if (!fullImage.isPointInside(badPoint))
        ThrowRDE("Bad point not inside image.");

      badPixels.emplace_back(y << 16 | x);
    }

    // Read rects
    for (auto i = 0U; i < badRectCount; ++i) {
      const DummyROIOpcode dummy(ri, bs);

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
  explicit TrimBounds(const RawImage& ri, ByteStream* bs)
      : ROIOpcode(ri, bs, false) {}

  void apply(const RawImage& ri) override { ri->subFrame(getRoi()); }
};

// ****************************************************************************

class DngOpcodes::PixelOpcode : public ROIOpcode {
  uint32 firstPlane;
  uint32 planes;
  uint32 rowPitch;
  uint32 colPitch;

protected:
  explicit PixelOpcode(const RawImage& ri, ByteStream* bs)
      : ROIOpcode(ri, bs, false) {
    firstPlane = bs->getU32();
    planes = bs->getU32();

    if (planes == 0 || firstPlane > ri->getCpp() || planes > ri->getCpp() ||
        firstPlane + planes > ri->getCpp()) {
      ThrowRDE("Bad plane params (first %u, num %u), got planes = %u",
               firstPlane, planes, ri->getCpp());
    }

    rowPitch = bs->getU32();
    colPitch = bs->getU32();

    const iRectangle2D& ROI = getRoi();

    if (rowPitch < 1 || rowPitch > static_cast<uint32>(ROI.getHeight()) ||
        colPitch < 1 || colPitch > static_cast<uint32>(ROI.getWidth()))
      ThrowRDE("Invalid pitch");
  }

  // traverses the current ROI and applies the operation OP to each pixel,
  // i.e. each pixel value v is replaced by op(x, y, v), where x/y are the
  // coordinates of the pixel value v.
  template <typename T, typename OP> void applyOP(const RawImage& ri, OP op) {
    int cpp = ri->getCpp();
    const iRectangle2D& ROI = getRoi();
    for (auto y = ROI.getTop(); y < ROI.getBottom(); y += rowPitch) {
      auto* src = reinterpret_cast<T*>(ri->getData(0, y));
      // Add offset, so this is always first plane
      src += firstPlane;
      for (auto x = ROI.getLeft(); x < ROI.getRight(); x += colPitch) {
        for (auto p = 0U; p < planes; ++p)
          src[x * cpp + p] = op(x, y, src[x * cpp + p]);
      }
    }
  }
};

// ****************************************************************************

class DngOpcodes::LookupOpcode : public PixelOpcode {
protected:
  vector<ushort16> lookup;

  explicit LookupOpcode(const RawImage& ri, ByteStream* bs)
      : PixelOpcode(ri, bs), lookup(65536) {}

  void setup(const RawImage& ri) override {
    PixelOpcode::setup(ri);
    if (ri->getDataType() != TYPE_USHORT16)
      ThrowRDE("Only 16 bit images supported");
  }

  void apply(const RawImage& ri) override {
    applyOP<ushort16>(
        ri, [this](uint32 x, uint32 y, ushort16 v) { return lookup[v]; });
  }
};

// ****************************************************************************

class DngOpcodes::TableMap final : public LookupOpcode {
public:
  explicit TableMap(const RawImage& ri, ByteStream* bs) : LookupOpcode(ri, bs) {
    auto count = bs->getU32();

    if (count == 0 || count > 65536)
      ThrowRDE("Invalid size of lookup table");

    for (auto i = 0U; i < count; ++i)
      lookup[i] = bs->getU16();

    if (count < lookup.size())
      fill_n(&lookup[count], lookup.size() - count, lookup[count - 1]);
  }
};

// ****************************************************************************

class DngOpcodes::PolynomialMap final : public LookupOpcode {
public:
  explicit PolynomialMap(const RawImage& ri, ByteStream* bs)
      : LookupOpcode(ri, bs) {
    vector<double> polynomial;

    const auto polynomial_size = bs->getU32() + 1UL;
    bs->check(8UL * polynomial_size);
    if (polynomial_size > 9)
      ThrowRDE("A polynomial with more than 8 degrees not allowed");

    polynomial.reserve(polynomial_size);
    std::generate_n(std::back_inserter(polynomial), polynomial_size,
                    [&bs]() { return bs->get<double>(); });

    // Create lookup
    lookup.resize(65536);
    for (auto i = 0U; i < lookup.size(); ++i) {
      double val = polynomial[0];
      for (auto j = 1U; j < polynomial.size(); ++j)
        val += polynomial[j] * pow(i / 65536.0, j);
      lookup[i] = (clampBits(static_cast<int>(val * 65535.5), 16));
    }
  }
};

// ****************************************************************************

class DngOpcodes::DeltaRowOrColBase : public PixelOpcode {
public:
  struct SelectX {
    static inline uint32 select(uint32 x, uint32 /*y*/) { return x; }
  };

  struct SelectY {
    static inline uint32 select(uint32 /*x*/, uint32 y) { return y; }
  };

protected:
  DeltaRowOrColBase(const RawImage& ri, ByteStream* bs) : PixelOpcode(ri, bs) {}
};

template <typename S>
class DngOpcodes::DeltaRowOrCol : public DeltaRowOrColBase {
public:
  void setup(const RawImage& ri) override {
    PixelOpcode::setup(ri);

    // If we are working on a float image, no need to convert to int
    if (ri->getDataType() != TYPE_USHORT16)
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

  // only meaningful for ushort16 images!
  virtual bool valueIsOk(float value) = 0;

  DeltaRowOrCol(const RawImage& ri, ByteStream* bs, float f2iScale_)
      : DeltaRowOrColBase(ri, bs), f2iScale(f2iScale_) {
    const auto deltaF_count = bs->getU32();
    bs->check(deltaF_count, 4);

    deltaF.reserve(deltaF_count);
    std::generate_n(std::back_inserter(deltaF), deltaF_count, [&bs]() {
      const auto F = bs->get<float>();
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

  bool valueIsOk(float value) final { return std::abs(value) <= absLimit; }

public:
  explicit OffsetPerRowOrCol(const RawImage& ri, ByteStream* bs)
      : DeltaRowOrCol<S>(ri, bs, 65535.0F),
        absLimit(double(std::numeric_limits<ushort16>::max()) /
                 this->f2iScale) {}

  void apply(const RawImage& ri) override {
    if (ri->getDataType() == TYPE_USHORT16) {
      this->template applyOP<ushort16>(
          ri, [this](uint32 x, uint32 y, ushort16 v) {
            return clampBits(this->deltaI[S::select(x, y)] + v, 16);
          });
    } else {
      this->template applyOP<float>(ri, [this](uint32 x, uint32 y, float v) {
        return this->deltaF[S::select(x, y)] + v;
      });
    }
  }
};

template <typename S>
class DngOpcodes::ScalePerRowOrCol final : public DeltaRowOrCol<S> {
  // We have pixel value in range of [0..65535]. We scale by float X.
  // For this to generate a value within the same range , the scale X needs
  // to be in the range [0..65535]. Since the offset is multiplied
  // by f2iScale before applying, we need to divide by f2iScale here.
  // So the maxLimit has the same value as absLimit for OffsetPerRowOrCol.
  static constexpr const double minLimit = 0.0;
  const double maxLimit;

  bool valueIsOk(float value) final {
    return value >= minLimit && value <= maxLimit;
  }

public:
  explicit ScalePerRowOrCol(const RawImage& ri, ByteStream* bs)
      : DeltaRowOrCol<S>(ri, bs, 1024.0F),
        maxLimit(double(std::numeric_limits<ushort16>::max()) /
                 this->f2iScale) {}

  void apply(const RawImage& ri) override {
    if (ri->getDataType() == TYPE_USHORT16) {
      this->template applyOP<ushort16>(ri, [this](uint32 x, uint32 y,
                                                  ushort16 v) {
        return clampBits((this->deltaI[S::select(x, y)] * v + 512) >> 10, 16);
      });
    } else {
      this->template applyOP<float>(ri, [this](uint32 x, uint32 y, float v) {
        return this->deltaF[S::select(x, y)] * v;
      });
    }
  }
};

// ****************************************************************************

DngOpcodes::DngOpcodes(const RawImage& ri, TiffEntry* entry) {
  ByteStream bs = entry->getData();

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
    } catch (std::out_of_range&) {
      ThrowRDE("Unknown unhandled Opcode: %d", code);
    }

    if (opConstructor != nullptr)
      opcodes.emplace_back(opConstructor(ri, &opcode_bs));
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

// defined here as empty destrutor, otherwise we'd need a complete definition
// of the the DngOpcode type in DngOpcodes.h
DngOpcodes::~DngOpcodes() = default;

void DngOpcodes::applyOpCodes(const RawImage& ri) {
  for (const auto& code : opcodes) {
    code->setup(ri);
    code->apply(ri);
  }
}

template <class Opcode>
std::unique_ptr<DngOpcodes::DngOpcode>
DngOpcodes::constructor(const RawImage& ri, ByteStream* bs) {
  return std::make_unique<Opcode>(ri, bs);
}

// ALL opcodes specified in DNG Specification MUST be listed here.
// however, some of them might not be implemented.
const std::map<uint32, std::pair<const char*, DngOpcodes::constructor_t>>
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
