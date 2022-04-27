/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2010 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real
    Copyright (C) 2017 Roman Lebedev

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

#include "decompressors/SamsungV0Decompressor.h"
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Common.h"                // for signExtend
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32
#include "io/ByteStream.h"                // for ByteStream
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstdint>                        // for uint32_t, uint16_t, int32_t
#include <iterator>                       // for advance, begin, end, next
#include <utility>                        // for swap
#include <vector>                         // for vector

namespace rawspeed {

SamsungV0Decompressor::SamsungV0Decompressor(RawImageData *image,
                                             const ByteStream& bso,
                                             const ByteStream& bsr)
    : AbstractSamsungDecompressor(image) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  const uint32_t width = mRaw->dim.x;
  const uint32_t height = mRaw->dim.y;

  if (width == 0 || height == 0 || width < 16 || width > 5546 || height > 3714)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  computeStripes(bso.peekStream(height, 4), bsr);
}

// FIXME: this is very close to IiqDecoder::computeSripes()
void SamsungV0Decompressor::computeStripes(ByteStream bso, ByteStream bsr) {
  const uint32_t height = mRaw->dim.y;

  std::vector<uint32_t> offsets;
  offsets.reserve(1 + height);
  for (uint32_t y = 0; y < height; y++)
    offsets.emplace_back(bso.getU32());
  offsets.emplace_back(bsr.getSize());

  stripes.reserve(height);

  auto offset_iterator = std::begin(offsets);
  bsr.skipBytes(*offset_iterator);

  auto next_offset_iterator = std::next(offset_iterator);
  while (next_offset_iterator < std::end(offsets)) {
    if (*offset_iterator >= *next_offset_iterator)
      ThrowRDE("Line offsets are out of sequence or slice is empty.");

    const auto size = *next_offset_iterator - *offset_iterator;
    assert(size > 0);

    stripes.emplace_back(bsr.getStream(size));

    std::advance(offset_iterator, 1);
    std::advance(next_offset_iterator, 1);
  }

  assert(stripes.size() == height);
}

void SamsungV0Decompressor::decompress() const {
  for (int row = 0; row < mRaw->dim.y; row++)
    decompressStrip(row, stripes[row]);

  // Swap red and blue pixels to get the final CFA pattern
  auto *rawU16 = dynamic_cast<RawImageDataU16*>(mRaw);
  assert(rawU16);
  const Array2DRef<uint16_t> out(rawU16->getU16DataAsUncroppedArray2DRef());
  for (int row = 0; row < out.height - 1; row += 2) {
    for (int col = 0; col < out.width - 1; col += 2)
      std::swap(out(row, col + 1), out(row + 1, col));
  }
}

int32_t SamsungV0Decompressor::calcAdj(BitPumpMSB32& bits, int nbits) {
  if (!nbits)
    return 0;
  return signExtend(bits.getBits(nbits), nbits);
}

void SamsungV0Decompressor::decompressStrip(int row,
                                            const ByteStream& bs) const {
  auto *rawU16 = dynamic_cast<RawImageDataU16*>(mRaw);
  assert(rawU16);
  const Array2DRef<uint16_t> out(rawU16->getU16DataAsUncroppedArray2DRef());
  assert(out.width > 0);

  BitPumpMSB32 bits(bs);

  std::array<int, 4> len;
  for (int& i : len)
    i = row < 2 ? 7 : 4;

  // Image is arranged in groups of 16 pixels horizontally
  for (int col = 0; col < out.width; col += 16) {
    bits.fill();
    bool dir = !!bits.getBitsNoFill(1);

    std::array<int, 4> op;
    for (int& i : op)
      i = bits.getBitsNoFill(2);

    for (int i = 0; i < 4; i++) {
      assert(op[i] >= 0 && op[i] <= 3);

      switch (op[i]) {
      case 3:
        len[i] = bits.getBits(4);
        break;
      case 2:
        len[i]--;
        break;
      case 1:
        len[i]++;
        break;
      default:
        // FIXME: it can be zero too.
        break;
      }

      if (len[i] < 0)
        ThrowRDE("Bit length less than 0.");
      if (len[i] > 16)
        ThrowRDE("Bit Length more than 16.");
    }

    if (dir) {
      // Upward prediction

      if (row < 2)
        ThrowRDE("Upward prediction for the first two rows. Raw corrupt");

      if (col + 16 >= out.width)
        ThrowRDE("Upward prediction for the last block of pixels. Raw corrupt");

      // First we decode even pixels
      for (int c = 0; c < 16; c += 2) {
        int b = len[c >> 3];
        int32_t adj = calcAdj(bits, b);

        out(row, col + c) = adj + out(row - 1, col + c);
      }

      // Now we decode odd pixels
      // Why on earth upward prediction only looks up 1 line above
      // is beyond me, it will hurt compression a deal.
      for (int c = 1; c < 16; c += 2) {
        int b = len[2 | (c >> 3)];
        int32_t adj = calcAdj(bits, b);

        out(row, col + c) = adj + out(row - 2, col + c);
      }
    } else {
      // Left to right prediction
      // First we decode even pixels
      int pred_left = col != 0 ? out(row, col - 2) : 128;
      for (int c = 0; c < 16; c += 2) {
        int b = len[c >> 3];
        int32_t adj = calcAdj(bits, b);

        if (col + c < out.width)
          out(row, col + c) = adj + pred_left;
      }

      // Now we decode odd pixels
      pred_left = col != 0 ? out(row, col - 1) : 128;
      for (int c = 1; c < 16; c += 2) {
        int b = len[2 | (c >> 3)];
        int32_t adj = calcAdj(bits, b);

        if (col + c < out.width)
          out(row, col + c) = adj + pred_left;
      }
    }
  }
}

} // namespace rawspeed
