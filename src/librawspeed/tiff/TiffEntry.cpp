/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2015 Pedro Côrte-Real
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

#include "rawspeedconfig.h"
#include "tiff/TiffEntry.h"
#include "adt/Casts.h"
#include "adt/NotARational.h"
#include "common/Common.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "parsers/TiffParserException.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <string>

namespace rawspeed {

// order see TiffDataType
const std::array<uint32_t, 14> TiffEntry::datashifts = {0, 0, 0, 1, 2, 3, 0,
                                                        0, 1, 2, 3, 2, 3, 2};
//                                  0-1-2-3-4-5-6-7-8-9-10-11-12-13

void TiffEntry::anchor() const {
  // Empty out-of-line definition for the purpose of anchoring
  // the class's vtable to this Translational Unit.
}

void TiffEntryWithData::anchor() const {
  // Empty out-of-line definition for the purpose of anchoring
  // the class's vtable to this Translational Unit.
}

TiffEntry::TiffEntry(TiffIFD* parent_, ByteStream& bs)
    : parent(parent_), tag(static_cast<TiffTag>(bs.getU16())) {
  const uint16_t numType = bs.getU16();
  if (numType > static_cast<uint16_t>(TiffDataType::OFFSET))
    ThrowTPE("Error reading TIFF structure. Unknown Type 0x%x encountered.",
             numType);
  type = static_cast<TiffDataType>(numType);
  count = bs.getU32();

  // check for count << datashift overflow
  if (count > UINT32_MAX >> datashifts[numType])
    ThrowTPE("integer overflow in size calculation.");

  uint32_t byte_size = count << datashifts[numType];
  uint32_t data_offset = UINT32_MAX;

  if (byte_size <= 4) {
    data_offset = bs.getPosition();
    data = bs.getSubStream(bs.getPosition(), byte_size);
    bs.skipBytes(4);
  } else {
    data_offset = bs.getU32();
    if (type == TiffDataType::OFFSET ||
        isIn(tag, {TiffTag::DNGPRIVATEDATA, TiffTag::MAKERNOTE,
                   TiffTag::MAKERNOTE_ALT, TiffTag::FUJI_RAW_IFD,
                   TiffTag::SUBIFDS, TiffTag::EXIFIFDPOINTER})) {
      // preserve offset for SUB_IFD/EXIF/MAKER_NOTE data
      if constexpr ((false)) {
        // limit access to range from 0 to data_offset+byte_size
        data = bs.getSubStream(data_offset, byte_size);
      } else {
        // allow access to whole file, necessary if offsets inside the maker
        // note point to outside data, which is forbidden due to the TIFF/DNG
        // spec but may happen none the less (see e.g. "old" ORF files like
        // EX-1, note: the tags outside of the maker note area are currently not
        // used anyway)
        data = bs;
        data.setPosition(data_offset);
        (void)data.check(byte_size);
      }
    } else {
      data = bs.getSubStream(data_offset, byte_size);
    }
  }
}

TiffEntry::TiffEntry(TiffIFD* parent_, TiffTag tag_, TiffDataType type_,
                     uint32_t count_, ByteStream data_)
    : parent(parent_), data(data_), tag(tag_), type(type_), count(count_) {
  // check for count << datashift overflow
  if (count > UINT32_MAX >> datashifts[static_cast<uint32_t>(type)])
    ThrowTPE("integer overflow in size calculation.");

  uint32_t bytesize = count << datashifts[static_cast<uint32_t>(type)];

  if (data.getSize() != bytesize)
    ThrowTPE("data set larger than entry size given");
}

void TiffEntry::setData(ByteStream data_) { data = data_; }

TiffEntryWithData::TiffEntryWithData(TiffIFD* parent_, TiffTag tag_,
                                     TiffDataType type_, uint32_t count_,
                                     Buffer mirror)
    : TiffEntry(parent_, tag_, type_, /*count=*/0, ByteStream()),
      data(mirror.begin(), mirror.end()) {
  setData(ByteStream(DataBuffer(
      Buffer(data.data(), implicit_cast<Buffer::size_type>(data.size())),
      Endianness::little)));
  count = count_;
}

bool RAWSPEED_READONLY TiffEntry::isInt() const {
  using enum TiffDataType;
  return type == LONG || type == SHORT || type == BYTE;
}

bool RAWSPEED_READONLY TiffEntry::isString() const {
  return type == TiffDataType::ASCII;
}

bool RAWSPEED_READONLY TiffEntry::isFloat() const {
  switch (type) {
    using enum TiffDataType;
  case FLOAT:
  case DOUBLE:
  case RATIONAL:
  case SRATIONAL:
  case LONG:
  case SLONG:
  case SHORT:
  case SSHORT:
    return true;
  default:
    return false;
  }
}

bool RAWSPEED_READONLY TiffEntry::isRational() const {
  switch (type) {
    using enum TiffDataType;
  case SHORT:
  case LONG:
  case RATIONAL:
    return true;
  default:
    return false;
  }
}

bool RAWSPEED_READONLY TiffEntry::isSRational() const {
  switch (type) {
    using enum TiffDataType;
  case SSHORT:
  case SLONG:
  case SRATIONAL:
    return true;
  default:
    return false;
  }
}

uint8_t TiffEntry::getByte(uint32_t index) const {
  if (type != TiffDataType::BYTE && type != TiffDataType::UNDEFINED)
    ThrowTPE("Wrong type %u encountered. Expected Byte on 0x%x",
             static_cast<unsigned>(type), static_cast<unsigned>(tag));

  return data.peekByte(index);
}

uint16_t TiffEntry::getU16(uint32_t index) const {
  if (type != TiffDataType::SHORT && type != TiffDataType::UNDEFINED)
    ThrowTPE("Wrong type %u encountered. Expected Short or Undefined on 0x%x",
             static_cast<unsigned>(type), static_cast<unsigned>(tag));

  return data.peek<uint16_t>(index);
}

int16_t TiffEntry::getI16(uint32_t index) const {
  if (type != TiffDataType::SSHORT && type != TiffDataType::UNDEFINED)
    ThrowTPE("Wrong type %u encountered. Expected Short or Undefined on 0x%x",
             static_cast<unsigned>(type), static_cast<unsigned>(tag));

  return data.peek<int16_t>(index);
}

uint32_t TiffEntry::getU32(uint32_t index) const {
  using enum TiffDataType;
  if (type == SHORT)
    return getU16(index);

  switch (type) {
  case LONG:
  case OFFSET:
  case BYTE:
  case UNDEFINED:
  case RATIONAL:
    break;
  default:
    ThrowTPE("Wrong type %u encountered. Expected Long, Offset, Rational or "
             "Undefined on 0x%x",
             static_cast<unsigned>(type), static_cast<unsigned>(tag));
  }

  return data.peek<uint32_t>(index);
}

int32_t TiffEntry::getI32(uint32_t index) const {
  using enum TiffDataType;
  if (type == SSHORT)
    return getI16(index);
  if (type != SLONG && type != SRATIONAL && type != UNDEFINED)
    ThrowTPE("Wrong type %u encountered. Expected SLong or Undefined on 0x%x",
             static_cast<unsigned>(type), static_cast<unsigned>(tag));

  return data.peek<int32_t>(index);
}

NotARational<uint32_t> TiffEntry::getRational(uint32_t index) const {
  if (!isRational()) {
    ThrowTPE("Wrong type 0x%x encountered. Expected Rational",
             static_cast<unsigned>(type));
  }

  if (type != TiffDataType::RATIONAL)
    return {getU32(index), 1};

  auto a = getU32(index * 2);
  auto b = getU32(index * 2 + 1);
  return {a, b};
}

NotARational<int32_t> TiffEntry::getSRational(uint32_t index) const {
  if (!isSRational()) {
    ThrowTPE("Wrong type 0x%x encountered. Expected SRational",
             static_cast<unsigned>(type));
  }

  if (type != TiffDataType::SRATIONAL)
    return {getI32(index), 1};

  auto a = getI32(index * 2);
  auto b = getI32(index * 2 + 1);
  return {a, b};
}

float TiffEntry::getFloat(uint32_t index) const {
  if (!isFloat()) {
    ThrowTPE("Wrong type 0x%x encountered. Expected Float or something "
             "convertible on 0x%x",
             static_cast<unsigned>(type), static_cast<unsigned>(tag));
  }

  switch (type) {
    using enum TiffDataType;
  case DOUBLE:
    return implicit_cast<float>(data.peek<double>(index));
  case FLOAT:
    return data.peek<float>(index);
  case LONG:
  case SHORT:
    return static_cast<float>(getU32(index));
  case SLONG:
  case SSHORT:
    return static_cast<float>(getI32(index));
  case RATIONAL: {
    auto r = getRational(index);
    return r.den ? static_cast<float>(r) : 0.0F;
  }
  case SRATIONAL: {
    auto r = getSRational(index);
    return r.den ? static_cast<float>(r) : 0.0F;
  }
  default:
    // unreachable
    return 0.0F;
  }
}

std::string TiffEntry::getString() const {
  if (type != TiffDataType::ASCII && type != TiffDataType::BYTE)
    ThrowTPE("Wrong type 0x%x encountered. Expected Ascii or Byte",
             static_cast<unsigned>(type));

  // *NOT* ByteStream::peekString() !
  Buffer tmp = data.peekBuffer(data.getRemainSize());
  const auto* termIter = std::find(tmp.begin(), tmp.end(), '\0');
  return {reinterpret_cast<const char*>(tmp.begin()),
          reinterpret_cast<const char*>(termIter)};
}

DataBuffer TiffEntry::getRootIfdData() const {
  const TiffIFD* p = parent;
  const TiffRootIFD* r = nullptr;
  while (p) {
    r = dynamic_cast<const TiffRootIFD*>(p);
    if (r)
      break;
    p = p->parent;
  }
  if (!r)
    ThrowTPE("Internal error in TiffIFD data structure.");

  assert(r != nullptr);
  return r->rootBuffer;
}

} // namespace rawspeed
