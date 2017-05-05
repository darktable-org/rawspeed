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
#include "common/Point.h"                 // for iPoint2D, iRectangle2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for RawDecoderException (ptr o...
#include "io/ByteStream.h"                // for ByteStream
#include "io/Endianness.h"                // for getHostEndianness, Endiann...
#include "tiff/TiffEntry.h"               // for TiffEntry
#include <algorithm>                      // for fill_n
#include <cassert>                        // for assert
#include <cmath>                          // for pow
#include <stdexcept>                      // for out_of_range
#include <tuple>                          // for tie, tuple

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
  explicit FixBadPixelsConstant(ByteStream* bs) {
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

class DngOpcodes::FixBadPixelsList final : public DngOpcodes::DngOpcode {
  std::vector<uint32> badPixels;

public:
  explicit FixBadPixelsList(ByteStream* bs) {
    bs->getU32(); // Skip phase - we don't care
    auto badPointCount = bs->getU32();
    auto badRectCount = bs->getU32();

    // Read points
    for (auto i = 0U; i < badPointCount; ++i) {
      auto y = bs->getU32();
      auto x = bs->getU32();
      badPixels.push_back(y << 16 | x);
    }

    // Read rects
    for (auto i = 0U; i < badRectCount; ++i) {
      auto top = bs->getU32();
      auto left = bs->getU32();
      auto bottom = bs->getU32();
      auto right = bs->getU32();
      for (auto y = top; y <= bottom; ++y) {
        for (auto x = left; x <= right; ++x) {
          badPixels.push_back(y << 16 | x);
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

class DngOpcodes::ROIOpcode : public DngOpcodes::DngOpcode {
  iRectangle2D roi;

protected:
  explicit ROIOpcode(ByteStream* bs) {
    uint32 top = bs->getU32();
    uint32 left = bs->getU32();
    uint32 bottom = bs->getU32();
    uint32 right = bs->getU32();

    roi = iRectangle2D(left, top, right - left, bottom - top);
  }

  const iRectangle2D& __attribute__((pure)) getRoi() const { return roi; }

  void setup(const RawImage& ri) override {
    iRectangle2D fullImage(0, 0, ri->dim.x, ri->dim.y);

    if (!roi.isThisInside(fullImage))
      ThrowRDE("Area of interest not inside image.");
  }
};

// ****************************************************************************

class DngOpcodes::TrimBounds final : public ROIOpcode {
public:
  explicit TrimBounds(ByteStream* bs) : ROIOpcode(bs) {}

  void apply(const RawImage& ri) override { ri->subFrame(getRoi()); }
};

// ****************************************************************************

class DngOpcodes::PixelOpcode : public ROIOpcode {
  uint32 firstPlane;
  uint32 planes;
  uint32 rowPitch;
  uint32 colPitch;

protected:
  explicit PixelOpcode(ByteStream* bs) : ROIOpcode(bs) {
    firstPlane = bs->getU32();
    planes = bs->getU32();
    rowPitch = bs->getU32();
    colPitch = bs->getU32();

    if (planes == 0)
      ThrowRDE("Zero planes");
    if (rowPitch == 0 || colPitch == 0)
      ThrowRDE("Invalid pitch");
  }

  void setup(const RawImage& ri) override {
    ROIOpcode::setup(ri);
    if (firstPlane + planes > ri->getCpp())
      ThrowRDE("Not that many planes in actual image");
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

  explicit LookupOpcode(ByteStream* bs) : PixelOpcode(bs), lookup(65536) {}

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
  explicit TableMap(ByteStream* bs) : LookupOpcode(bs) {
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
  explicit PolynomialMap(ByteStream* bs) : LookupOpcode(bs) {
    vector<double> polynomial;

    polynomial.resize(bs->getU32() + 1UL);

    if (polynomial.size() > 9)
      ThrowRDE("A polynomial with more than 8 degrees not allowed");

    for (auto& coeff : polynomial)
      coeff = bs->get<double>();

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
  vector<float> deltaF;
  vector<int> deltaI;

  DeltaRowOrColBase(ByteStream* bs, float f2iScale) : PixelOpcode(bs) {
    deltaF.resize(bs->getU32());

    for (auto& f : deltaF)
      f = bs->get<float>();

    deltaI.reserve(deltaF.size());
    for (auto f : deltaF)
      deltaI.emplace_back(static_cast<int>(f2iScale * f));
  }
};

// ****************************************************************************

template <typename S>
class DngOpcodes::OffsetPerRowOrCol final : public DeltaRowOrColBase {
public:
  explicit OffsetPerRowOrCol(ByteStream* bs)
      : DeltaRowOrColBase(bs, 65535.0F) {}

  void apply(const RawImage& ri) override {
    if (ri->getDataType() == TYPE_USHORT16) {
      applyOP<ushort16>(ri, [this](uint32 x, uint32 y, ushort16 v) {
        return clampBits(deltaI[S::select(x, y)] + v, 16);
      });
    } else {
      applyOP<float>(ri, [this](uint32 x, uint32 y, float v) {
        return deltaF[S::select(x, y)] + v;
      });
    }
  }
};

template <typename S>
class DngOpcodes::ScalePerRowOrCol final : public DeltaRowOrColBase {
public:
  explicit ScalePerRowOrCol(ByteStream* bs) : DeltaRowOrColBase(bs, 1024.0F) {}

  void apply(const RawImage& ri) override {
    if (ri->getDataType() == TYPE_USHORT16) {
      applyOP<ushort16>(ri, [this](uint32 x, uint32 y, ushort16 v) {
        return clampBits((deltaI[S::select(x, y)] * v + 512) >> 10, 16);
      });
    } else {
      applyOP<float>(ri, [this](uint32 x, uint32 y, float v) {
        return deltaF[S::select(x, y)] * v;
      });
    }
  }
};

// ****************************************************************************

DngOpcodes::DngOpcodes(TiffEntry* entry) {
  ByteStream bs = entry->getData();
  // DNG opcodes are always stored in big-endian byte order.
  bs.setInNativeByteOrder(getHostEndianness() == big);

  const auto opcode_count = bs.getU32();
  opcodes.reserve(opcode_count);

  for (auto i = 0U; i < opcode_count; i++) {
    auto code = bs.getU32();
    bs.getU32(); // ignore version
#ifdef DEBUG
    bs.getU32(); // ignore flags
#else
    auto flags = bs.getU32();
#endif
    auto expected_pos = bs.getU32() + bs.getPosition();

    const char* opName = nullptr;
    constructor_t opConstructor = nullptr;
    try {
      std::tie(opName, opConstructor) = Map.at(code);
    } catch (std::out_of_range&) {
      ThrowRDE("Unknown unhandled Opcode: %d", code);
    }

    if (opConstructor != nullptr)
      opcodes.emplace_back(opConstructor(&bs));
    else {
#ifndef DEBUG
      // Throw Error if not marked as optional
      if (!(flags & 1))
#endif
        ThrowRDE("Unsupported Opcode: %d (%s)", code, opName);
    }

    if (bs.getPosition() != expected_pos)
      ThrowRDE("Inconsistent length of opcode");
  }

  assert(opcodes.size() == opcode_count);
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
std::unique_ptr<DngOpcodes::DngOpcode> DngOpcodes::constructor(ByteStream* bs) {
  return make_unique<Opcode>(bs);
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
