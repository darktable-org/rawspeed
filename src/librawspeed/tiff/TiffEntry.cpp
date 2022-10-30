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

#include "tiff/TiffEntry.h"
#include "common/Common.h"               // for uint32_t, int16_t, uint16_t
#include "common/NotARational.h"
#include "parsers/TiffParserException.h" // for ThrowTPE
#include "tiff/TiffIFD.h"                // for TiffIFD, TiffRootIFD
#include "tiff/TiffTag.h"                // for TiffTag, DNGPRIVATEDATA
#include <cassert>                       // for assert
#include <cstdint>                       // for UINT32_MAX
#include <cstring>                       // for strnlen
#include <initializer_list>              // for initializer_list
#include <string>                        // for string
#include <utility>                       // for move

namespace rawspeed {

class DataBuffer;

// order see TiffDataType
const std::array<uint32_t, 14> TiffEntry::datashifts = {0, 0, 0, 1, 2, 3, 0,
                                                        0, 1, 2, 3, 2, 3, 2};
//                                  0-1-2-3-4-5-6-7-8-9-10-11-12-13

TiffEntry::TiffEntry(TiffIFD* parent_, ByteStream& bs)
    : parent(parent_), tag(static_cast<TiffTag>(bs.getU16())) {
  const uint16_t numType = bs.getU16();
  if (numType > static_cast<uint16_t>(TiffDataType::OFFSET))
    ThrowTPE("Error reading TIFF structure. Unknown Type 0x%x encountered.", numType);
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
#if 0
      // limit access to range from 0 to data_offset+byte_size
      data = ByteStream(bs, data_offset, byte_size, bs.getByteOrder());
#else
      // allow access to whole file, necessary if offsets inside the maker note
      // point to outside data, which is forbidden due to the TIFF/DNG spec but
      // may happen none the less (see e.g. "old" ORF files like EX-1, note:
      // the tags outside of the maker note area are currently not used anyway)
      data = bs;
      data.setPosition(data_offset);
      (void)data.check(byte_size);
#endif
    } else {
      data = bs.getSubStream(data_offset, byte_size);
    }
  }
}

TiffEntry::TiffEntry(TiffIFD* parent_, TiffTag tag_, TiffDataType type_,
                     uint32_t count_, ByteStream&& data_)
    : parent(parent_), data(std::move(data_)), tag(tag_), type(type_),
      count(count_) {
  // check for count << datashift overflow
  if (count > UINT32_MAX >> datashifts[static_cast<uint32_t>(type)])
    ThrowTPE("integer overflow in size calculation.");

  uint32_t bytesize = count << datashifts[static_cast<uint32_t>(type)];

  if (data.getSize() != bytesize)
    ThrowTPE("data set larger than entry size given");
}

bool __attribute__((pure)) TiffEntry::isInt() const {
  return type == TiffDataType::LONG || type == TiffDataType::SHORT ||
         type == TiffDataType::BYTE;
}

bool __attribute__((pure)) TiffEntry::isString() const {
  return type == TiffDataType::ASCII;
}

bool __attribute__((pure)) TiffEntry::isFloat() const {
  switch (type) {
  case TiffDataType::FLOAT:
  case TiffDataType::DOUBLE:
  case TiffDataType::RATIONAL:
  case TiffDataType::SRATIONAL:
  case TiffDataType::LONG:
  case TiffDataType::SLONG:
  case TiffDataType::SHORT:
  case TiffDataType::SSHORT:
    return true;
  default:
    return false;
  }
}

bool __attribute__((pure)) TiffEntry::isSRational() const {
  switch (type) {
  case TiffDataType::SRATIONAL:
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
  if (type == TiffDataType::SHORT)
    return getU16(index);

  switch (type) {
  case TiffDataType::LONG:
  case TiffDataType::OFFSET:
  case TiffDataType::BYTE:
  case TiffDataType::UNDEFINED:
  case TiffDataType::RATIONAL:
  case TiffDataType::SRATIONAL:
    break;
  default:
    ThrowTPE("Wrong type %u encountered. Expected Long, Offset, Rational or "
             "Undefined on 0x%x",
             static_cast<unsigned>(type), static_cast<unsigned>(tag));
  }

  return data.peek<uint32_t>(index);
}

int32_t TiffEntry::getI32(uint32_t index) const {
  if (type == TiffDataType::SSHORT)
    return getI16(index);
  if (type != TiffDataType::SLONG && type != TiffDataType::UNDEFINED)
    ThrowTPE("Wrong type %u encountered. Expected SLong or Undefined on 0x%x",
             static_cast<unsigned>(type), static_cast<unsigned>(tag));

  return data.peek<int32_t>(index);
}

NotARational<int> TiffEntry::getSRational(uint32_t index) const {
  if (!isSRational()) {
    ThrowTPE("Wrong type 0x%x encountered. Expected SRational",
             static_cast<unsigned>(type));
  }
  auto a = static_cast<int>(getU32(index * 2));
  auto b = static_cast<int>(getU32(index * 2 + 1));
  return {a, b};
}

float TiffEntry::getFloat(uint32_t index) const {
  if (!isFloat()) {
    ThrowTPE("Wrong type 0x%x encountered. Expected Float or something "
             "convertible on 0x%x",
             static_cast<unsigned>(type), static_cast<unsigned>(tag));
  }

  switch (type) {
  case TiffDataType::DOUBLE:
    return data.peek<double>(index);
  case TiffDataType::FLOAT:
    return data.peek<float>(index);
  case TiffDataType::LONG:
  case TiffDataType::SHORT:
    return static_cast<float>(getU32(index));
  case TiffDataType::SLONG:
  case TiffDataType::SSHORT:
    return static_cast<float>(getI32(index));
  case TiffDataType::RATIONAL: {
    uint32_t a = getU32(index * 2);
    uint32_t b = getU32(index * 2 + 1);
    return b != 0 ? static_cast<float>(a) / b : 0.0F;
  }
  case TiffDataType::SRATIONAL: {
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
  const auto bufSize = data.getRemainSize();
  const auto* buf = data.peekData(bufSize);
  const auto* s = reinterpret_cast<const char*>(buf);
  return {s, strnlen(s, bufSize)};
}

const DataBuffer &TiffEntry::getRootIfdData() const {
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
