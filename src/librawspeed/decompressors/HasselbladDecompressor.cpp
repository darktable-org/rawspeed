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
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Common.h"                // for to_array
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowException, ThrowRDE
#include "decompressors/HuffmanTable.h"   // for HuffmanTableLUT, HuffmanTable
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32, BitStream<>:...
#include "io/ByteStream.h"                // for ByteStream
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstdint>                        // for uint16_t

namespace rawspeed {

HasselbladDecompressor::HasselbladDecompressor(const ByteStream& bs,
                                               const RawImage& img)
    : AbstractLJpegDecompressor(bs, img) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  // FIXME: could be wrong. max "active pixels" - "100 MP"
  if (mRaw->dim.x == 0 || mRaw->dim.y == 0 || mRaw->dim.x % 2 != 0 ||
      mRaw->dim.x > 12000 || mRaw->dim.y > 8816) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }
}

// Returns len bits as a signed value.
// Highest bit is a sign bit
inline int HasselbladDecompressor::getBits(BitPumpMSB32& bs, int len) {
  if (!len)
    return 0;
  int diff = bs.getBits(len);
  diff = HuffmanTable::extend(diff, len);
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

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  assert(out.height > 0);
  assert(out.width > 0);
  assert(out.width % 2 == 0);

  const auto ht = to_array<1>(getHuffmanTables(1));
  ht[0]->verifyCodeSymbolsAreValidDiffLenghts();

  BitPumpMSB32 bitStream(input);
  // Pixels are packed two at a time, not like LJPEG:
  // [p1_length_as_huffman][p2_length_as_huffman][p0_diff_with_length][p1_diff_with_length]|NEXT PIXELS
  for (int row = 0; row < out.height; row++) {
    int p1 = 0x8000 + pixelBaseOffset;
    int p2 = 0x8000 + pixelBaseOffset;
    for (int col = 0; col < out.width; col += 2) {
      int len1 = ht[0]->decodeCodeValue(bitStream);
      int len2 = ht[0]->decodeCodeValue(bitStream);
      p1 += getBits(bitStream, len1);
      p2 += getBits(bitStream, len2);
      // NOTE: this is rather unusual and weird, but appears to be correct.
      // clampBits(p, 16) results in completely garbled images.
      out(row, col) = uint16_t(p1);
      out(row, col + 1) = uint16_t(p2);
    }
  }
  input.skipBytes(bitStream.getStreamPosition());
}

void HasselbladDecompressor::decode(int pixelBaseOffset_)
{
  pixelBaseOffset = pixelBaseOffset_;

  if (pixelBaseOffset < -65536 || pixelBaseOffset > 65535)
    ThrowRDE("Either the offset %i or the bounds are wrong.", pixelBaseOffset);

  // We cannot use fully decoding huffman table,
  // because values are packed two pixels at the time.
  fullDecodeHT = false;

  AbstractLJpegDecompressor::decode();
}

} // namespace rawspeed
