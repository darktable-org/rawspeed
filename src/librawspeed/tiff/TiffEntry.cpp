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

#include "common/StdAfx.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include <math.h>

using namespace std;

namespace RawSpeed {

// order see TiffDataType
static const uint32 datashifts[] = {0,0,0,1,2,3,0,0,1,2, 3, 2, 3, 2};
//                                  0-1-2-3-4-5-6-7-8-9-10-11-12-13

TiffEntry::TiffEntry(ByteStream &bs) {
  tag = (TiffTag)bs.getShort();
  const ushort16 numType = bs.getShort();
  if (numType > TIFF_OFFSET)
    ThrowTPE("Error reading TIFF structure. Unknown Type 0x%x encountered.", numType);
  type = (TiffDataType) numType;
  count = bs.getUInt();

  // check for count << datashift overflow
  if (count > UINT32_MAX >> datashifts[type])
    ThrowTPE("Parse error in TiffEntry: integer overflow in size calculation.");

  uint32 byte_size = count << datashifts[type];
  uint32 data_offset = UINT32_MAX;

  if (byte_size <= 4) {
    data_offset = bs.getPosition();
    data = bs.getSubStream(bs.getPosition(), byte_size);
    bs.skipBytes(4);
  } else {
    data_offset = bs.getUInt();
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


TiffEntry::TiffEntry(TiffTag tag, TiffDataType type, uint32 count, ByteStream&& _data)
  : data(std::move(_data)), tag(tag), type(type), count(count)
{
  // check for count << datashift overflow
  if (count > UINT32_MAX >> datashifts[type])
    ThrowTPE("Parse error in TiffEntry: integer overflow in size calculation.");

  uint32 bytesize = count << datashifts[type];

  if (data.getSize() != bytesize)
    ThrowTPE("TIFF, data set larger than entry size given");
}


bool TiffEntry::isInt() const {
  return type == TIFF_LONG || type == TIFF_SHORT || type == TIFF_BYTE;
}

bool TiffEntry::isString() const {
  return type == TIFF_ASCII;
}

bool TiffEntry::isFloat() const {
  return  (type == TIFF_FLOAT || type == TIFF_DOUBLE || type == TIFF_RATIONAL ||
           type == TIFF_SRATIONAL || type == TIFF_LONG || type == TIFF_SLONG ||
           type == TIFF_SHORT || type == TIFF_SSHORT);
}

uchar8 TiffEntry::getByte(uint32 num) const {
  if (type != TIFF_BYTE && type != TIFF_UNDEFINED)
    ThrowTPE("TIFF, getByte: Wrong type %u encountered. Expected Byte on 0x%x", type, tag);

  return data.peekByte(num);
}

ushort16 TiffEntry::getShort(uint32 num) const {
  if (type != TIFF_SHORT && type != TIFF_UNDEFINED)
    ThrowTPE("TIFF, getShort: Wrong type %u encountered. Expected Short or Undefined on 0x%x", type, tag);

  return data.peek<ushort16>(num);
}

short16 TiffEntry::getSShort(uint32 num) const {
  if (type != TIFF_SSHORT && type != TIFF_UNDEFINED)
    ThrowTPE("TIFF, getSShort: Wrong type %u encountered. Expected Short or Undefined on 0x%x", type, tag);

  return data.peek<short16>(num);
}

uint32 TiffEntry::getInt(uint32 num) const {
  if (type == TIFF_SHORT)
    return getShort(num);
  if (!(type == TIFF_LONG || type == TIFF_OFFSET || type == TIFF_BYTE || type == TIFF_UNDEFINED || type == TIFF_RATIONAL || type == TIFF_SRATIONAL))
    ThrowTPE("TIFF, getInt: Wrong type %u encountered. Expected Long, Offset, Rational or Undefined on 0x%x", type, tag);

  return data.peek<uint32>(num);
}

int32 TiffEntry::getSInt(uint32 num) const {
  if (type == TIFF_SSHORT)
    return getSShort(num);
  if (!(type == TIFF_SLONG || type == TIFF_UNDEFINED))
    ThrowTPE("TIFF, getSInt: Wrong type %u encountered. Expected SLong or Undefined on 0x%x", type, tag);

  return data.peek<int32>(num);
}

void TiffEntry::getShortArray(ushort16 *array, uint32 num) const {
  for (uint32 i = 0; i < num; i++)
    array[i] = getShort(i);
}

void TiffEntry::getIntArray(uint32 *array, uint32 num) const {
  for (uint32 i = 0; i < num; i++)
    array[i] = getInt(i);
}

void TiffEntry::getFloatArray(float *array, uint32 num) const {
  for (uint32 i = 0; i < num; i++)
    array[i] = getFloat(i);
}

float TiffEntry::getFloat(uint32 num) const {
  if (!isFloat())
    ThrowTPE("TIFF, getFloat: Wrong type 0x%x encountered. Expected Float or something convertible on 0x%x", type, tag);

  switch (type) {
  case TIFF_DOUBLE: return data.peek<double>(num);
  case TIFF_FLOAT:  return data.peek<float>(num);
  case TIFF_LONG:
  case TIFF_SHORT:  return (float)getInt(num);
  case TIFF_SLONG:
  case TIFF_SSHORT: return (float)getSInt(num);
  case TIFF_RATIONAL: {
    uint32 a = getInt(num*2);
    uint32 b = getInt(num*2+1);
    return b ? (float) a/b : 0.f;
  }
  case TIFF_SRATIONAL: {
    int a = (int) getInt(num*2);
    int b = (int) getInt(num*2+1);
    return b ? (float) a/b : 0.f;
  }
  default:
    // unreachable
    return 0.0f;
  }
}

string TiffEntry::getString() const {
  if (type != TIFF_ASCII && type != TIFF_BYTE)
    ThrowTPE("TIFF, getString: Wrong type 0x%x encountered. Expected Ascii or Byte", type);

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
