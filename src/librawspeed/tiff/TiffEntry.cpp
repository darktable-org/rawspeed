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
#include "common/Common.h"               // for uint32, ushort16, int32
#include "parsers/TiffParserException.h" // for ThrowTPE
#include "tiff/TiffIFD.h"                // for TiffIFD, TiffRootIFD
#include "tiff/TiffTag.h"                // for ::DNGPRIVATEDATA, ::EXIFIFD...
#include <algorithm>                     // for move
#include <cstdint>                       // for UINT32_MAX
#include <cstring>                       // for strnlen
#include <string>                        // for string

using namespace std;

namespace RawSpeed {

class DataBuffer;

// order see TiffDataType
const uint32 TiffEntry::datashifts[] = {0, 0, 0, 1, 2, 3, 0,
                                        0, 1, 2, 3, 2, 3, 2};
//                                  0-1-2-3-4-5-6-7-8-9-10-11-12-13

TiffEntry::TiffEntry(TiffIFD* parent_, ByteStream& bs) : parent(parent_) {
  tag = (TiffTag)bs.getU16();
  const ushort16 numType = bs.getU16();
  if (numType > TIFF_OFFSET)
    ThrowTPE("Error reading TIFF structure. Unknown Type 0x%x encountered.", numType);
  type = (TiffDataType) numType;
  count = bs.getU32();

  // check for count << datashift overflow
  if (count > UINT32_MAX >> datashifts[type])
    ThrowTPE("integer overflow in size calculation.");

  uint32 byte_size = count << datashifts[type];
  uint32 data_offset = UINT32_MAX;

  if (byte_size <= 4) {
    data_offset = bs.getPosition();
    data = bs.getSubStream(bs.getPosition(), byte_size);
    bs.skipBytes(4);
  } else {
    data_offset = bs.getU32();
    if (type == TIFF_OFFSET || isIn(tag, {DNGPRIVATEDATA, MAKERNOTE, MAKERNOTE_ALT, FUJI_RAW_IFD, SUBIFDS, EXIFIFDPOINTER})) {
      // preserve offset for SUB_IFD/EXIF/MAKER_NOTE data
#if 0
      // limit access to range from 0 to data_offset+byte_size
      data = ByteStream(bs, data_offset, byte_size, bs.isInNativeByteOrder());
#else
      // allow access to whole file, necesary if offsets inside the maker note
      // point to outside data, which is forbidden due to the TIFF/DNG spec but
      // may happen none the less (see e.g. "old" ORF files like EX-1, note:
      // the tags outside of the maker note area are currently not used anyway)
      data = bs;
      data.setPosition(data_offset);
#endif
    } else {
      data = bs.getSubStream(data_offset, byte_size);
    }
  }
}

TiffEntry::TiffEntry(TiffIFD* parent_, TiffTag tag_, TiffDataType type_,
                     uint32 count_, ByteStream&& data_)
    : parent(parent_), data(std::move(data_)), tag(tag_), type(type_),
      count(count_) {
  // check for count << datashift overflow
  if (count > UINT32_MAX >> datashifts[type])
    ThrowTPE("integer overflow in size calculation.");

  uint32 bytesize = count << datashifts[type];

  if (data.getSize() != bytesize)
    ThrowTPE("data set larger than entry size given");
}

bool __attribute__((pure)) TiffEntry::isInt() const {
  return type == TIFF_LONG || type == TIFF_SHORT || type == TIFF_BYTE;
}

bool __attribute__((pure)) TiffEntry::isString() const {
  return type == TIFF_ASCII;
}

bool __attribute__((pure)) TiffEntry::isFloat() const {
  return  (type == TIFF_FLOAT || type == TIFF_DOUBLE || type == TIFF_RATIONAL ||
           type == TIFF_SRATIONAL || type == TIFF_LONG || type == TIFF_SLONG ||
           type == TIFF_SHORT || type == TIFF_SSHORT);
}

uchar8 TiffEntry::getByte(uint32 index) const {
  if (type != TIFF_BYTE && type != TIFF_UNDEFINED)
    ThrowTPE("Wrong type %u encountered. Expected Byte on 0x%x", type, tag);

  return data.peekByte(index);
}

ushort16 TiffEntry::getU16(uint32 index) const {
  if (type != TIFF_SHORT && type != TIFF_UNDEFINED)
    ThrowTPE("Wrong type %u encountered. Expected Short or Undefined on 0x%x",
             type, tag);

  return data.peek<ushort16>(index);
}

short16 TiffEntry::getI16(uint32 index) const {
  if (type != TIFF_SSHORT && type != TIFF_UNDEFINED)
    ThrowTPE("Wrong type %u encountered. Expected Short or Undefined on 0x%x",
             type, tag);

  return data.peek<short16>(index);
}

uint32 TiffEntry::getU32(uint32 index) const {
  if (type == TIFF_SHORT)
    return getU16(index);
  if (!(type == TIFF_LONG || type == TIFF_OFFSET || type == TIFF_BYTE ||
        type == TIFF_UNDEFINED || type == TIFF_RATIONAL ||
        type == TIFF_SRATIONAL)) {
    ThrowTPE("Wrong type %u encountered. Expected Long, Offset, Rational or "
             "Undefined on 0x%x",
             type, tag);
  }

  return data.peek<uint32>(index);
}

int32 TiffEntry::getI32(uint32 index) const {
  if (type == TIFF_SSHORT)
    return getI16(index);
  if (!(type == TIFF_SLONG || type == TIFF_UNDEFINED))
    ThrowTPE("Wrong type %u encountered. Expected SLong or Undefined on 0x%x",
             type, tag);

  return data.peek<int32>(index);
}

float TiffEntry::getFloat(uint32 index) const {
  if (!isFloat()) {
    ThrowTPE("Wrong type 0x%x encountered. Expected Float or something "
             "convertible on 0x%x",
             type, tag);
  }

  switch (type) {
  case TIFF_DOUBLE: return data.peek<double>(index);
  case TIFF_FLOAT:  return data.peek<float>(index);
  case TIFF_LONG:
  case TIFF_SHORT:  return (float)getU32(index);
  case TIFF_SLONG:
  case TIFF_SSHORT: return (float)getI32(index);
  case TIFF_RATIONAL: {
    uint32 a = getU32(index*2);
    uint32 b = getU32(index*2+1);
    return b ? (float) a/b : 0.f;
  }
  case TIFF_SRATIONAL: {
    auto a = (int)getU32(index * 2);
    auto b = (int)getU32(index * 2 + 1);
    return b ? (float) a/b : 0.f;
  }
  default:
    // unreachable
    return 0.0f;
  }
}

string TiffEntry::getString() const {
  if (type != TIFF_ASCII && type != TIFF_BYTE)
    ThrowTPE("Wrong type 0x%x encountered. Expected Ascii or Byte", type);

  const char* s = data.peekString();
  return string(s, strnlen(s, count));
}

const DataBuffer &TiffEntry::getRootIfdData() const {
  TiffIFD* p = parent;
  TiffRootIFD* r = nullptr;
  while (p && !(r = dynamic_cast<TiffRootIFD*>(p)))
    p = p->parent;
  if (!r)
    ThrowTPE("Internal error in TiffIFD data structure.");
  return r->rootBuffer;
}

} // namespace RawSpeed
