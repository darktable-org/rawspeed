/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser

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
#include "common/Common.h"                // for uint32, ushort16, make_unique
#include "common/Point.h"                 // for iPoint2D, iRectangle2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for RawDecoderException (ptr o...
#include "io/ByteStream.h"                // for ByteStream
#include "io/Endianness.h"                // for getHostEndianness, Endiann...
#include "tiff/TiffEntry.h"               // for TiffEntry
#include <algorithm>                      // for fill_n
#include <cmath>                          // for pow

using std::vector;
using std::fill_n;

namespace RawSpeed {

class DngOpcodes::DngOpcode {
public:
  virtual ~DngOpcode() = default;

  // Will be called once before processing.
  // Can be used for preparing pre-calculated values, etc.
  virtual void setup(const RawImage& ri) {
    // NOP by default. child class shall override this if needed.
  }

  // Will be called for actual processing.
  virtual void apply(RawImage& ri) = 0;
};

// ****************************************************************************

class DngOpcodes::FixBadPixelsConstant final : public DngOpcodes::DngOpcode {
  uint32 value;

public:
  explicit FixBadPixelsConstant(ByteStream& bs) {
    value = bs.getU32();
    bs.getU32(); // Bayer Phase not used
  }

  void setup(const RawImage& ri) override {
    // These limitations are present within the DNG SDK as well.
    if (ri->getDataType() != TYPE_USHORT16)
      ThrowRDE("Only 16 bit images supported");

    if (ri->getCpp() > 1)
      ThrowRDE("Only 1 component images supported");
  }

  void apply(RawImage& ri) override {
    iPoint2D crop = ri->getCropOffset();
    uint32 offset = crop.x | (crop.y << 16);
    for (auto y = 0; y < ri->dim.y; ++y) {
      auto* src = (ushort16*)ri->getData(0, y);
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
  explicit FixBadPixelsList(ByteStream& bs) {
    bs.getU32(); // Skip phase - we don't care
    auto badPointCount = bs.getU32();
    auto badRectCount = bs.getU32();

    // Read points
    for (auto i = 0u; i < badPointCount; ++i) {
      auto y = bs.getU32();
      auto x = bs.getU32();
      badPixels.push_back(y << 16 | x);
    }

    // Read rects
    for (auto i = 0u; i < badRectCount; ++i) {
      auto top = bs.getU32();
      auto left = bs.getU32();
      auto bottom = bs.getU32();
      auto right = bs.getU32();
      for (auto y = top; y <= bottom; ++y) {
        for (auto x = left; x <= right; ++x) {
          badPixels.push_back(y << 16 | x);
        }
      }
    }
  }

  void apply(RawImage& ri) override {
    ri->mBadPixelPositions.insert(ri->mBadPixelPositions.begin(),
                                  badPixels.begin(), badPixels.end());
  }
};

// ****************************************************************************

class DngOpcodes::ROIOpcode : public DngOpcodes::DngOpcode {
protected:
  uint32 top, left, bottom, right;

  explicit ROIOpcode(ByteStream& bs) {
    top = bs.getU32();
    left = bs.getU32();
    bottom = bs.getU32();
    right = bs.getU32();
  }

  void setup(const RawImage& ri) override {
    iRectangle2D roi(left, top, right - left, bottom - top);
    iRectangle2D fullImage(0, 0, ri->dim.x, ri->dim.y);

    if (!roi.isThisInside(fullImage))
      ThrowRDE("Area of interest not inside image.");
  }
};

// ****************************************************************************

class DngOpcodes::TrimBounds final : public ROIOpcode {
public:
  explicit TrimBounds(ByteStream& bs) : ROIOpcode(bs) {}

  void apply(RawImage& ri) override {
    ri->subFrame(iRectangle2D(left, top, right - left, bottom - top));
  }
};

// ****************************************************************************

class DngOpcodes::PixelOpcode : public ROIOpcode {
protected:
  uint32 firstPlane, planes, rowPitch, colPitch;

  explicit PixelOpcode(ByteStream& bs) : ROIOpcode(bs) {
    firstPlane = bs.getU32();
    planes = bs.getU32();
    rowPitch = bs.getU32();
    colPitch = bs.getU32();

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
  template <typename T, typename OP> void applyOP(RawImage& ri, OP op) {
    int cpp = ri->getCpp();
    for (auto y = top; y < bottom; y += rowPitch) {
      auto* src = (T*)ri->getData(0, y);
      // Add offset, so this is always first plane
      src += firstPlane;
      for (auto x = left; x < right; x += colPitch) {
        for (auto p = 0u; p < planes; ++p)
          src[x * cpp + p] = op(x, y, src[x * cpp + p]);
      }
    }
  }
};

// ****************************************************************************

class DngOpcodes::LookupOpcode : public PixelOpcode {
protected:
  vector<ushort16> lookup;

  explicit LookupOpcode(ByteStream& bs) : PixelOpcode(bs), lookup(65536) {}

  void setup(const RawImage& ri) override {
    PixelOpcode::setup(ri);
    if (ri->getDataType() != TYPE_USHORT16)
      ThrowRDE("Only 16 bit images supported");
  }

  void apply(RawImage& ri) override {
    applyOP<ushort16>(
        ri, [this](uint32 x, uint32 y, ushort16 v) { return lookup[v]; });
  }
};

// ****************************************************************************

class DngOpcodes::TableMap final : public LookupOpcode {
public:
  explicit TableMap(ByteStream& bs) : LookupOpcode(bs) {
    auto count = bs.getU32();

    if (count == 0 || count > 65536)
      ThrowRDE("Invalid size of lookup table");

    for (auto i = 0u; i < count; ++i)
      lookup[i] = bs.getU16();

    if (count < lookup.size())
      fill_n(&lookup[count], lookup.size() - count, lookup[count - 1]);
  }
};

// ****************************************************************************

class DngOpcodes::PolynomialMap final : public LookupOpcode {
public:
  explicit PolynomialMap(ByteStream& bs) : LookupOpcode(bs) {
    vector<double> polynomial;

    polynomial.resize(bs.getU32() + 1UL);

    if (polynomial.size() > 9)
      ThrowRDE("A polynomial with more than 8 degrees not allowed");

    for (auto& coeff : polynomial)
      coeff = bs.get<double>();

    // Create lookup
    lookup.resize(65536);
    for (auto i = 0u; i < lookup.size(); ++i) {
      double val = polynomial[0];
      for (auto j = 1u; j < polynomial.size(); ++j)
        val += polynomial[j] * pow(i / 65536.0, j);
      lookup[i] = (clampBits((int)(val * 65535.5), 16));
    }
  }
};

// ****************************************************************************

class DngOpcodes::DeltaRowOrColBase : public PixelOpcode {
public:
  struct SelectX {
    static inline uint32 select(uint32 x, uint32 y) { return x; }
  };

  struct SelectY {
    static inline uint32 select(uint32 x, uint32 y) { return y; }
  };

protected:
  vector<float> deltaF;
  vector<int> deltaI;

  DeltaRowOrColBase(ByteStream& bs, float f2iScale) : PixelOpcode(bs) {
    deltaF.resize(bs.getU32());

    for (auto& f : deltaF)
      f = bs.get<float>();

    deltaI.reserve(deltaF.size());
    for (auto f : deltaF)
      deltaI.emplace_back((int)(f2iScale * f));
  }
};

// ****************************************************************************

template <typename S>
class DngOpcodes::OffsetPerRowOrCol final : public DeltaRowOrColBase {
public:
  explicit OffsetPerRowOrCol(ByteStream& bs)
      : DeltaRowOrColBase(bs, 65535.0f) {}

  void apply(RawImage& ri) override {
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
  explicit ScalePerRowOrCol(ByteStream& bs) : DeltaRowOrColBase(bs, 1024.0f) {}

  void apply(RawImage& ri) override {
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
  // DNG opcodes seem to be always stored in big endian
  bs.setInNativeByteOrder(getHostEndianness() == big);

  using OffsetPerRow = OffsetPerRowOrCol<DeltaRowOrColBase::SelectY>;
  using OffsetPerCol = OffsetPerRowOrCol<DeltaRowOrColBase::SelectX>;

  using ScalePerRow = ScalePerRowOrCol<DeltaRowOrColBase::SelectY>;
  using ScalePerCol = ScalePerRowOrCol<DeltaRowOrColBase::SelectX>;

  auto opcode_count = bs.getU32();
  for (auto i = 0u; i < opcode_count; i++) {
    auto code = bs.getU32();
    bs.getU32(); // ignore version
    auto flags = bs.getU32();
    auto expected_pos = bs.getU32() + bs.getPosition();

    switch (code) {
    case 4:
      opcodes.push_back(make_unique<FixBadPixelsConstant>(bs));
      break;
    case 5:
      opcodes.push_back(make_unique<FixBadPixelsList>(bs));
      break;
    case 6:
      opcodes.push_back(make_unique<TrimBounds>(bs));
      break;
    case 7:
      opcodes.push_back(make_unique<TableMap>(bs));
      break;
    case 8:
      opcodes.push_back(make_unique<PolynomialMap>(bs));
      break;
    case 10:
      opcodes.push_back(make_unique<OffsetPerRow>(bs));
      break;
    case 11:
      opcodes.push_back(make_unique<OffsetPerCol>(bs));
      break;
    case 12:
      opcodes.push_back(make_unique<ScalePerRow>(bs));
      break;
    case 13:
      opcodes.push_back(make_unique<ScalePerCol>(bs));
      break;
    default:
      // Throw Error if not marked as optional
      if (!(flags & 1))
        ThrowRDE("Unsupported Opcode: %d", code);
    }
    if (bs.getPosition() != expected_pos)
      ThrowRDE("Inconsistent length of opcode");
  }
}

// defined here as empty destrutor, otherwise we'd need a complete definition
// of the the DngOpcode type in DngOpcodes.h
DngOpcodes::~DngOpcodes() = default;

void DngOpcodes::applyOpCodes(RawImage& ri) {
  for (const auto& code : opcodes) {
    code->setup(ri);
    code->apply(ri);
  }
}

} // namespace RawSpeed
