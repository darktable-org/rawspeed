/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2017-2018 Roman Lebedev

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

#include "tiff/CiffEntry.h"
#include "common/NORangesSet.h"          // for set
#include "io/Buffer.h"                   // for Buffer
#include "io/ByteStream.h"               // for ByteStream
#include "parsers/CiffParserException.h" // for ThrowCPE
#include <string>                        // for string
#include <utility>                       // for pair
#include <vector>                        // for vector

using std::string;
using std::vector;

namespace rawspeed {

CiffEntry::CiffEntry(NORangesSet<Buffer>* valueDatas,
                     const ByteStream* valueData, ByteStream dirEntry) {
  uint16_t p = dirEntry.getU16();

  // NOLINTNEXTLINE cppcoreguidelines-prefer-member-initializer
  tag = static_cast<CiffTag>(p & 0x3fff);
  uint16_t datalocation = (p & 0xc000);
  // NOLINTNEXTLINE cppcoreguidelines-prefer-member-initializer
  type = static_cast<CiffDataType>(p & 0x3800);

  uint32_t data_offset;
  uint32_t bytesize;

  switch (datalocation) {
  case 0x0000:
    // Data is offset in value_data
    bytesize = dirEntry.getU32();
    data_offset = dirEntry.getU32();
    data = valueData->getSubStream(data_offset, bytesize);
    if (!valueDatas->emplace(data).second)
      ThrowCPE("Two valueData's overlap. Raw corrupt!");
    break;
  case 0x4000:
    // Data is stored directly in entry
    data_offset = dirEntry.getPosition();
    // Maximum of 8 bytes of data (the size and offset fields)
    bytesize = 8;
    data = dirEntry.getStream(bytesize);
    break;
  default:
    ThrowCPE("Don't understand data location 0x%x", datalocation);
  }

  // Set the number of items using the shift
  count = bytesize >> getElementShift();
}

uint32_t __attribute__((pure)) CiffEntry::getElementShift() const {
  switch (type) {
    case CIFF_SHORT:
      return 1;
    case CIFF_LONG:
    case CIFF_MIX:
    case CIFF_SUB1:
    case CIFF_SUB2:
      return 2;
    default:
      // e.g. CIFF_BYTE or CIFF_ASCII
      return 0;
  }
}

uint32_t __attribute__((pure)) CiffEntry::getElementSize() const {
  switch (type) {
    case CIFF_BYTE:
    case CIFF_ASCII:
      return 1;
    case CIFF_SHORT:
      return 2;
    case CIFF_LONG:
    case CIFF_MIX:
    case CIFF_SUB1:
    case CIFF_SUB2:
      return 4;
    default:
      return 0;
  }
}

bool __attribute__((pure)) CiffEntry::isInt() const {
  return (type == CIFF_LONG || type == CIFF_SHORT || type ==  CIFF_BYTE);
}

uint32_t CiffEntry::getU32(uint32_t num) const {
  if (!isInt()) {
    ThrowCPE(
        "Wrong type 0x%x encountered. Expected Long, Short or Byte at 0x%x",
        type, tag);
  }

  if (type == CIFF_BYTE)
    return getByte(num);
  if (type == CIFF_SHORT)
    return getU16(num);

  return data.peek<uint32_t>(num);
}

uint16_t CiffEntry::getU16(uint32_t num) const {
  if (type != CIFF_SHORT && type != CIFF_BYTE)
    ThrowCPE("Wrong type 0x%x encountered. Expected Short at 0x%x", type, tag);

  return data.peek<uint16_t>(num);
}

uint8_t CiffEntry::getByte(uint32_t num) const {
  if (type != CIFF_BYTE)
    ThrowCPE("Wrong type 0x%x encountered. Expected Byte at 0x%x", type, tag);

  return data.peek<uint8_t>(num);
}

string CiffEntry::getString() const {
  if (type != CIFF_ASCII)
    ThrowCPE("Wrong type 0x%x encountered. Expected Ascii", type);

  if (count == 0)
    return "";

  return data.peekString();
}

vector<string> CiffEntry::getStrings() const {
  if (type != CIFF_ASCII)
    ThrowCPE("Wrong type 0x%x encountered. Expected Ascii", type);

  const string str(reinterpret_cast<const char*>(data.peekData(count)), count);

  vector<string> strs;

  uint32_t start = 0;
  for (uint32_t i = 0; i < count; i++) {
    if (str[i] != 0)
      continue;

    strs.emplace_back(reinterpret_cast<const char*>(&str[start]));
    start = i + 1;
  }

  return strs;
}

bool __attribute__((pure)) CiffEntry::isString() const {
  return (type == CIFF_ASCII);
}

} // namespace rawspeed
