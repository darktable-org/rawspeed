/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2018 Roman Lebedev

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

#include "rawspeedconfig.h"
#include "adt/NORangesSet.h"
#include "io/ByteStream.h"
#include "tiff/CiffTag.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rawspeed {

class Buffer;
class CiffIFD; // IWYU pragma: keep
template <typename T> class NORangesSet;

/*
 * Tag data type information.
 */
enum class CiffDataType : uint16_t {
  BYTE = 0x0000,  /* 8-bit unsigned integer */
  ASCII = 0x0800, /* 8-bit bytes w/ last byte null */
  SHORT = 0x1000, /* 16-bit unsigned integer */
  LONG = 0x1800,  /* 32-bit unsigned integer */
  MIX = 0x2000,   /* 32-bit unsigned integer */
  SUB1 = 0x2800,  /* 32-bit unsigned integer */
  SUB2 = 0x3000,  /* 32-bit unsigned integer */

};

class CiffEntry final {
  friend class CiffIFD;

  ByteStream data;

  CiffEntry(ByteStream data, CiffTag tag, CiffDataType type, uint32_t count);

public:
  static CiffEntry Create(NORangesSet<Buffer>* valueDatas, ByteStream valueData,
                          ByteStream dirEntry);

  [[nodiscard]] ByteStream getData() const { return data; }

  [[nodiscard]] uint8_t getByte(uint32_t num = 0) const;
  [[nodiscard]] uint32_t getU32(uint32_t num = 0) const;
  [[nodiscard]] uint16_t getU16(uint32_t num = 0) const;

  [[nodiscard]] std::string_view getString() const;
  [[nodiscard]] std::vector<std::string> getStrings() const;

  [[nodiscard]] uint32_t RAWSPEED_READONLY getElementSize() const;

  [[nodiscard]] static uint32_t
  getElementShift(CiffDataType type) RAWSPEED_READONLY;

  // variables:
  CiffTag tag;
  CiffDataType type;
  uint32_t count;

  [[nodiscard]] bool RAWSPEED_READONLY isInt() const;
  [[nodiscard]] bool RAWSPEED_READONLY isString() const;
};

} // namespace rawspeed
