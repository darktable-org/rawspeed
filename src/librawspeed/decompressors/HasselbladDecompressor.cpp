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

#include "decompressors/HasselbladDecompressor.h"
#include "common/Common.h"                // for uint32, ushort16
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32, BitStream<>::f...
#include "io/ByteStream.h"                // for ByteStream
#include <array>                          // for array
#include <cassert>                        // for assert

namespace rawspeed {

// Returns len bits as a signed value.
// Highest bit is a sign bit
inline int HasselbladDecompressor::getBits(BitPumpMSB32* bs, int len) {
  int diff = bs->getBits(len);
  diff = len > 0 ? HuffmanTable::signExtended(diff, len) : diff;
  if (diff == 65535)
    return -32768;
  return diff;
}

void HasselbladDecompressor::decodeScan() {
  if (frame.w != static_cast<unsigned>(mRaw->dim.x) ||
      frame.h != static_cast<unsigned>(mRaw->dim.y)) {
    ThrowRDE("LJPEG frame does not match EXIF dimensions: (%u; %u) vs (%i; %i)",
             frame.w, frame.h, mRaw->dim.x, mRaw->dim.y);
  }

  assert(frame.h > 0);
  assert(frame.w > 0);
  assert(frame.w % 2 == 0);

  const auto ht = getHuffmanTables<1>();

  BitPumpMSB32 bitStream(input);
  // Pixels are packed two at a time, not like LJPEG:
  // [p1_length_as_huffman][p2_length_as_huffman][p0_diff_with_length][p1_diff_with_length]|NEXT PIXELS
  for (uint32 y = 0; y < frame.h; y++) {
    auto* dest = reinterpret_cast<ushort16*>(mRaw->getData(0, y));
    int p1 = 0x8000 + pixelBaseOffset;
    int p2 = 0x8000 + pixelBaseOffset;
    for (uint32 x = 0; x < frame.w; x += 2) {
      int len1 = ht[0]->decodeLength(bitStream);
      int len2 = ht[0]->decodeLength(bitStream);
      p1 += getBits(&bitStream, len1);
      p2 += getBits(&bitStream, len2);
      dest[x] = p1;
      dest[x+1] = p2;
    }
  }
  input.skipBytes(bitStream.getBufferPosition());
}

void HasselbladDecompressor::decode(int pixelBaseOffset_)
{
  pixelBaseOffset = pixelBaseOffset_;
  // We cannot use fully decoding huffman table,
  // because values are packed two pixels at the time.
  fullDecodeHT = false;

  AbstractLJpegDecompressor::decode();
}

} // namespace rawspeed
