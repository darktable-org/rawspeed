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

#include "rawspeedconfig.h"
#include "tiff/CiffEntry.h"
#include "adt/Invariant.h"
#include "adt/NORangesSet.h"
#include "adt/Optional.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "parsers/CiffParserException.h"
#include "tiff/CiffTag.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using std::vector;

namespace rawspeed {

CiffEntry::CiffEntry(ByteStream data_, CiffTag tag_, CiffDataType type_,
                     uint32_t count_)
    : data(data_), tag(tag_), type(type_), count(count_) {}

CiffEntry CiffEntry::Create(NORangesSet<Buffer>* valueDatas,
                            ByteStream valueData, ByteStream dirEntry) {
  uint16_t p = dirEntry.getU16();

  const auto tag = static_cast<CiffTag>(p & 0x3fff);
  uint16_t datalocation = (p & 0xc000);
  const auto type = static_cast<CiffDataType>(p & 0x3800);

  uint32_t data_offset;
  uint32_t bytesize;

  Optional<ByteStream> data;

  switch (datalocation) {
  case 0x0000:
    // Data is offset in value_data
    bytesize = dirEntry.getU32();
    data_offset = dirEntry.getU32();
    data = valueData.getSubStream(data_offset, bytesize);
    if (!valueDatas->insert(*data))
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
  invariant(data.has_value());

  // Set the number of items using the shift
  uint32_t count = bytesize >> getElementShift(type);

  return {*data, tag, type, count};
}

uint32_t CiffEntry::getElementShift(CiffDataType type) {
  switch (type) {
    using enum CiffDataType;
  case SHORT:
    return 1;
  case LONG:
  case MIX:
  case SUB1:
  case SUB2:
    return 2;
  default:
    // e.g. BYTE or ASCII
    return 0;
  }
}

uint32_t RAWSPEED_READONLY CiffEntry::getElementSize() const {
  switch (type) {
    using enum CiffDataType;
  case BYTE:
  case ASCII:
    return 1;
  case SHORT:
    return 2;
  case LONG:
  case MIX:
  case SUB1:
  case SUB2:
    return 4;
  }
  __builtin_unreachable();
}

bool RAWSPEED_READONLY CiffEntry::isInt() const {
  using enum CiffDataType;
  return (type == LONG || type == SHORT || type == BYTE);
}

uint32_t CiffEntry::getU32(uint32_t num) const {
  if (!isInt()) {
    ThrowCPE(
        "Wrong type 0x%x encountered. Expected Long, Short or Byte at 0x%x",
        static_cast<unsigned>(type), static_cast<unsigned>(tag));
  }

  if (type == CiffDataType::BYTE)
    return getByte(num);
  if (type == CiffDataType::SHORT)
    return getU16(num);

  return data.peek<uint32_t>(num);
}

uint16_t CiffEntry::getU16(uint32_t num) const {
  if (type != CiffDataType::SHORT && type != CiffDataType::BYTE)
    ThrowCPE("Wrong type 0x%x encountered. Expected Short at 0x%x",
             static_cast<unsigned>(type), static_cast<unsigned>(tag));

  return data.peek<uint16_t>(num);
}

uint8_t CiffEntry::getByte(uint32_t num) const {
  if (type != CiffDataType::BYTE)
    ThrowCPE("Wrong type 0x%x encountered. Expected Byte at 0x%x",
             static_cast<unsigned>(type), static_cast<unsigned>(tag));

  return data.peek<uint8_t>(num);
}

std::string_view CiffEntry::getString() const {
  if (type != CiffDataType::ASCII)
    ThrowCPE("Wrong type 0x%x encountered. Expected Ascii",
             static_cast<unsigned>(type));

  if (count == 0)
    return "";

  return data.peekString();
}

vector<std::string> CiffEntry::getStrings() const {
  if (type != CiffDataType::ASCII)
    ThrowCPE("Wrong type 0x%x encountered. Expected Ascii",
             static_cast<unsigned>(type));

  const std::string str(reinterpret_cast<const char*>(data.peekData(count)),
                        count);

  vector<std::string> strs;

  uint32_t start = 0;
  for (uint32_t i = 0; i < count; i++) {
    if (str[i] != 0)
      continue;

    strs.emplace_back(&str[start]);
    start = i + 1;
  }

  return strs;
}

bool RAWSPEED_READONLY CiffEntry::isString() const {
  return (type == CiffDataType::ASCII);
}

} // namespace rawspeed
