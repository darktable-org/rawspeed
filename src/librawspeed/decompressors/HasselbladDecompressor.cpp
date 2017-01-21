#include "common/StdAfx.h"
#include "decompressors/HasselbladDecompressor.h"

/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
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

namespace RawSpeed {

// Returns len bits as a signed value.
// Highest bit is a sign bit
inline static int getBits(BitPumpMSB32& bs, int len) {
  int diff = HuffmanTable::signExtended(bs.getBits(len), len);
  if (diff == 65535)
    return -32768;
  return diff;
}

void HasselbladDecompressor::decodeScan()
{
  BitPumpMSB32 bitStream(*input);
  // Pixels are packed two at a time, not like LJPEG:
  // [p1_length_as_huffman][p2_length_as_huffman][p0_diff_with_length][p1_diff_with_length]|NEXT PIXELS
  for (uint32 y = 0; y < frame.h; y++) {
    ushort16 *dest = (ushort16*) mRaw->getData(0, y);
    int p1 = 0x8000 + pixelBaseOffset;
    int p2 = 0x8000 + pixelBaseOffset;
    for (uint32 x = 0; x < frame.w; x += 2) {
      int len1 = huff[0]->decodeLength(bitStream);
      int len2 = huff[0]->decodeLength(bitStream);
      p1 += getBits(bitStream, len1);
      p2 += getBits(bitStream, len2);
      dest[x] = p1;
      dest[x+1] = p2;
    }
  }
  input->skipBytes(bitStream.getBufferPosition());
}

} // namespace RawSpeed
