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

#include "io/ByteStream.h" // for ByteStream
#include "tiff/TiffTag.h"  // for TiffTag
#include <array>           // for array
#include <cstdint>         // for uint32_t, uint16_t, uint8_t, int16_t, int...
#include <string>          // for string
#include <vector>          // for vector

namespace rawspeed {

class DataBuffer;

class TiffIFD;

/*
 * Tag data type information.
 *
 * Note: RATIONALs are the ratio of two 32-bit integer values.
 */
enum class TiffDataType {
  NOTYPE = 0,     /* placeholder */
  BYTE = 1,       /* 8-bit unsigned integer */
  ASCII = 2,      /* 8-bit bytes w/ last byte null */
  SHORT = 3,      /* 16-bit unsigned integer */
  LONG = 4,       /* 32-bit unsigned integer */
  RATIONAL = 5,   /* 64-bit unsigned fraction */
  SBYTE = 6,      /* !8-bit signed integer */
  UNDEFINED = 7,  /* !8-bit untyped data */
  SSHORT = 8,     /* !16-bit signed integer */
  SLONG = 9,      /* !32-bit signed integer */
  SRATIONAL = 10, /* !64-bit signed fraction */
  FLOAT = 11,     /* !32-bit IEEE floating point */
  DOUBLE = 12,    /* !64-bit IEEE floating point */
  OFFSET = 13,    /* 32-bit unsigned offset used for IFD and other offsets */
};

class TiffEntry
{
  TiffIFD* parent;
  ByteStream data;

  friend class TiffIFD;

  template <typename T, T (TiffEntry::*getter)(uint32_t index) const>
  [[nodiscard]] [[nodiscard]] [[nodiscard]] std::vector<T>
  getArray(uint32_t count_) const {
    std::vector<T> res(count_);
    for (uint32_t i = 0; i < count_; ++i)
      res[i] = (this->*getter)(i);
    return res;
  }

public:
  TiffTag tag;
  TiffDataType type;
  uint32_t count;

  TiffEntry(TiffIFD* parent, TiffTag tag, TiffDataType type, uint32_t count,
            ByteStream&& data);
  TiffEntry(TiffIFD* parent, ByteStream& bs);

  [[nodiscard]] bool __attribute__((pure)) isFloat() const;
  [[nodiscard]] bool __attribute__((pure)) isInt() const;
  [[nodiscard]] bool __attribute__((pure)) isString() const;
  [[nodiscard]] uint8_t getByte(uint32_t index = 0) const;
  [[nodiscard]] uint32_t getU32(uint32_t index = 0) const;
  [[nodiscard]] int32_t getI32(uint32_t index = 0) const;
  [[nodiscard]] uint16_t getU16(uint32_t index = 0) const;
  [[nodiscard]] int16_t getI16(uint32_t index = 0) const;
  [[nodiscard]] float getFloat(uint32_t index = 0) const;
  [[nodiscard]] std::string getString() const;

  [[nodiscard]] inline std::vector<uint16_t>
  getU16Array(uint32_t count_) const {
    return getArray<uint16_t, &TiffEntry::getU16>(count_);
  }

  [[nodiscard]] inline std::vector<uint32_t>
  getU32Array(uint32_t count_) const {
    return getArray<uint32_t, &TiffEntry::getU32>(count_);
  }

  [[nodiscard]] inline std::vector<float> getFloatArray(uint32_t count_) const {
    return getArray<float, &TiffEntry::getFloat>(count_);
  }

  [[nodiscard]] ByteStream getData() const { return data; }

  [[nodiscard]] const DataBuffer& getRootIfdData() const;

protected:
  static const std::array<uint32_t, 14> datashifts;
};

} // namespace rawspeed
