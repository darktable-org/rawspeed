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

#pragma once

#include "common/Common.h" // for uint32, uchar8, ushort16, int32, short16
#include "io/ByteStream.h" // for ByteStream
#include "tiff/TiffTag.h"  // for TiffTag
#include <string>          // for string
#include <vector>          // for vector

namespace RawSpeed {

class DataBuffer;

class TiffIFD;

/*
 * Tag data type information.
 *
 * Note: RATIONALs are the ratio of two 32-bit integer values.
 */
enum TiffDataType {
  TIFF_NOTYPE    = 0, /* placeholder */
  TIFF_BYTE      = 1, /* 8-bit unsigned integer */
  TIFF_ASCII     = 2, /* 8-bit bytes w/ last byte null */
  TIFF_SHORT     = 3, /* 16-bit unsigned integer */
  TIFF_LONG      = 4, /* 32-bit unsigned integer */
  TIFF_RATIONAL  = 5, /* 64-bit unsigned fraction */
  TIFF_SBYTE     = 6, /* !8-bit signed integer */
  TIFF_UNDEFINED = 7, /* !8-bit untyped data */
  TIFF_SSHORT    = 8, /* !16-bit signed integer */
  TIFF_SLONG     = 9, /* !32-bit signed integer */
  TIFF_SRATIONAL = 10, /* !64-bit signed fraction */
  TIFF_FLOAT     = 11, /* !32-bit IEEE floating point */
  TIFF_DOUBLE    = 12, /* !64-bit IEEE floating point */
  TIFF_OFFSET    = 13, /* 32-bit unsigned offset used for IFD and other offsets */
};

class TiffEntry
{
  TiffIFD* parent;
  ByteStream data;

  friend class TiffIFD;

  template <typename T, T (TiffEntry::* getter)(uint32) const>
  std::vector<T> getArray(uint32 count_) const
  {
    std::vector<T> res(count_);
    for (uint32 i = 0; i < count_; ++i)
      res[i] = (this->*getter)(i);
    return res;
  }

public:
  TiffTag tag;
  TiffDataType type;
  uint32 count;

  TiffEntry(TiffIFD* parent, TiffTag tag, TiffDataType type, uint32 count,
            ByteStream&& data);
  TiffEntry(TiffIFD* parent, ByteStream& bs);

  bool __attribute__((pure)) isFloat() const;
  bool __attribute__((pure)) isInt() const;
  bool __attribute__((pure)) isString() const;
  uchar8 getByte(uint32 index = 0) const;
  uint32 getU32(uint32 index = 0) const;
  int32 getI32(uint32 index = 0) const;
  ushort16 getU16(uint32 index = 0) const;
  short16 getI16(uint32 index = 0) const;
  float getFloat(uint32 index = 0) const;
  std::string getString() const;

  inline std::vector<ushort16> getU16Array(uint32 count_) const
  {
    return getArray<ushort16, &TiffEntry::getU16>(count_);
  }

  inline std::vector<uint32> getU32Array(uint32 count_) const
  {
    return getArray<uint32, &TiffEntry::getU32>(count_);
  }

  inline std::vector<float> getFloatArray(uint32 count_) const
  {
    return getArray<float, &TiffEntry::getFloat>(count_);
  }

  ByteStream& getData() { return data; }
  const uchar8* getData(uint32 size) { return data.getData(size); }

  const DataBuffer& getRootIfdData() const;

protected:
  static const uint32 datashifts[];
};

} // namespace RawSpeed
