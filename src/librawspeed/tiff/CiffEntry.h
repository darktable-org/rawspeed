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

#include "common/Common.h"      // for uint32, uchar8, ushort16
#include "common/NORangesSet.h" // for set
#include "io/ByteStream.h"      // for ByteStream
#include "tiff/CiffTag.h"       // for CiffTag
#include <string>               // for string
#include <vector>               // for vector

namespace rawspeed {

class Buffer;
class CiffIFD; // IWYU pragma: keep

/*
 * Tag data type information.
 */
enum CiffDataType {
	CIFF_BYTE  = 0x0000,	/* 8-bit unsigned integer */
	CIFF_ASCII = 0x0800,	/* 8-bit bytes w/ last byte null */
	CIFF_SHORT = 0x1000,	/* 16-bit unsigned integer */
	CIFF_LONG  = 0x1800,	/* 32-bit unsigned integer */
	CIFF_MIX   = 0x2000,	/* 32-bit unsigned integer */
	CIFF_SUB1  = 0x2800,	/* 32-bit unsigned integer */
	CIFF_SUB2  = 0x3000,	/* 32-bit unsigned integer */

};

class CiffEntry
{
  friend class CiffIFD;

  ByteStream data;

public:
  explicit CiffEntry(NORangesSet<Buffer>* valueDatas,
                     const ByteStream* valueData, ByteStream dirEntry);

  const ByteStream& getData() const { return data; }

  uchar8 getByte(uint32 num = 0) const;
  uint32 getU32(uint32 num = 0) const;
  ushort16 getU16(uint32 num = 0) const;

  std::string getString() const;
  std::vector<std::string> getStrings() const;

  uint32 __attribute__((pure)) getElementSize() const;
  uint32 __attribute__((pure)) getElementShift() const;

  // variables:
  CiffTag tag;
  CiffDataType type;
  uint32 count;

  bool __attribute__((pure)) isInt() const;
  bool __attribute__((pure)) isString() const;
};

} // namespace rawspeed
