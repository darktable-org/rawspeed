/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real

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
#include "common/Common.h"               // for uchar8, uint32, ushort16
#include "io/Endianness.h"               // for getU32LE, getU16LE
#include "parsers/CiffParserException.h" // for ThrowCPE
#include <cstdio>                        // for sprintf
#include <cstring>                       // for memcpy, strlen
#include <string>                        // for string, allocator
#include <vector>                        // for vector

using namespace std;

namespace RawSpeed {

CiffEntry::CiffEntry(FileMap* f, uint32 value_data, uint32 offset) {
  own_data = nullptr;
  ushort16 p = getU16LE(f->getData(offset, 2));
  tag = (CiffTag) (p & 0x3fff);
  ushort16 datalocation = (p & 0xc000);
  type = (CiffDataType) (p & 0x3800);
  if (datalocation == 0x0000) { // Data is offset in value_data
    bytesize = getU32LE(f->getData(offset + 2, 4));
    data_offset = getU32LE(f->getData(offset + 6, 4)) + value_data;
    data = f->getData(data_offset, bytesize);
  } else if (datalocation == 0x4000) { // Data is stored directly in entry
    data_offset = offset + 2;
    bytesize = 8; // Maximum of 8 bytes of data (the size and offset fields)
    data = f->getData(data_offset, bytesize);
  } else
    ThrowCPE("Don't understand data location 0x%x\n", datalocation);

  // Set the number of items using the shift
  count = bytesize >> getElementShift();
}

CiffEntry::~CiffEntry() {
  if (own_data)
    delete[] own_data;
}

uint32 __attribute__((pure)) CiffEntry::getElementShift() {
  switch (type) {
    case CIFF_BYTE:
    case CIFF_ASCII:
      return 0;
    case CIFF_SHORT:
      return 1;
    case CIFF_LONG:
    case CIFF_MIX:
    case CIFF_SUB1:
    case CIFF_SUB2:
      return 2;
  }
  return 0;
}

uint32 __attribute__((pure)) CiffEntry::getElementSize() {
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
  }
  return 0;
}

bool __attribute__((pure)) CiffEntry::isInt() {
  return (type == CIFF_LONG || type == CIFF_SHORT || type ==  CIFF_BYTE);
}

uint32 CiffEntry::getU32(uint32 num) {
  if (!isInt()) {
    ThrowCPE(
        "Wrong type 0x%x encountered. Expected Long, Short or Byte at 0x%x",
        type, tag);
  }

  if (type == CIFF_BYTE)
    return getByte(num);
  if (type == CIFF_SHORT)
    return getU16(num);

  if (num*4+3 >= bytesize)
    ThrowCPE("Trying to read out of bounds");

  return getU32LE(data + num * 4);
}

ushort16 CiffEntry::getU16(uint32 num) {
  if (type != CIFF_SHORT && type != CIFF_BYTE)
    ThrowCPE("Wrong type 0x%x encountered. Expected Short at 0x%x", type, tag);

  if (num*2+1 >= bytesize)
    ThrowCPE("Trying to read out of bounds");

  return getU16LE(data + num * 2);
}

uchar8 CiffEntry::getByte(uint32 num) {
  if (type != CIFF_BYTE)
    ThrowCPE("Wrong type 0x%x encountered. Expected Byte at 0x%x", type, tag);

  if (num >= bytesize)
    ThrowCPE("Trying to read out of bounds");

  return data[num];
}

string CiffEntry::getString() {
  if (type != CIFF_ASCII)
    ThrowCPE("Wrong type 0x%x encountered. Expected Ascii", type);
  if (!own_data) {
    own_data = new uchar8[count];
    memcpy(own_data, data, count);
    own_data[count-1] = 0;  // Ensure string is not larger than count defines
  }
  return string((const char*)&own_data[0]);
}

vector<string> CiffEntry::getStrings() {
  vector<string> strs;
  if (type != CIFF_ASCII)
    ThrowCPE("Wrong type 0x%x encountered. Expected Ascii", type);
  if (!own_data) {
    own_data = new uchar8[count];
    memcpy(own_data, data, count);
    own_data[count-1] = 0;  // Ensure string is not larger than count defines
  }
  uint32 start = 0;
  for (uint32 i=0; i< count; i++) {
    if (own_data[i] == 0) {
      strs.emplace_back((const char *)&own_data[start]);
      start = i+1;
    }
  }
  return strs;
}

bool __attribute__((pure)) CiffEntry::isString() {
  return (type == CIFF_ASCII);
}

void CiffEntry::setData( const void *in_data, uint32 byte_count )
{
  if (byte_count > bytesize)
    ThrowCPE("data set larger than entry size given");

  if (!own_data) {
    own_data = new uchar8[bytesize];
    memcpy(own_data, data, bytesize);
  }
  memcpy(own_data, in_data, byte_count);
}

#ifdef _MSC_VER
#pragma warning(disable: 4996) // this function or variable may be unsafe
#endif

std::string CiffEntry::getValueAsString()
{
  if (type == CIFF_ASCII)
    return string((const char*)&data[0]);
  auto *temp_string = new char[4096];
  if (count == 1) {
    switch (type) {
      case CIFF_LONG:
        sprintf(temp_string, "Long: %u (0x%x)", getU32(), getU32());
        break;
      case CIFF_SHORT:
        sprintf(temp_string, "Short: %u (0x%x)", getU32(), getU32());
        break;
      case CIFF_BYTE:
        sprintf(temp_string, "Byte: %u (0x%x)", getU32(), getU32());
        break;
      default:
        sprintf(temp_string, "Type: %x: ", type);
        for (uint32 i = 0; i < getElementSize(); i++) {
          sprintf(&temp_string[strlen(temp_string-1)], "%x", data[i]);
        }
    }
  }
  string ret(temp_string);
  delete [] temp_string;
  return ret;
}

} // namespace RawSpeed
