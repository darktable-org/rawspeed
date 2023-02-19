/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2023 Roman Lebedev

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

#include "decompressors/HasselbladDecompressor.h" // for HasselbladDecompressor, HasselbladSliceWidths
#include "adt/Array2DRef.h"                       // for Array2DRef
#include "adt/Point.h"                       // for iPoint2D, iPoint2D::area_...
#include "adt/iterator_range.h"              // for iterator_range
#include "common/RawImage.h"                 // for RawImage, RawImageData
#include "decoders/RawDecoderException.h"    // for ThrowException, ThrowRDE
#include "decompressors/DummyHuffmanTable.h" // for DummyHuffmanTable
#include "decompressors/HuffmanTableLUT.h"   // for HuffmanTableLUT
#include "io/BitPumpJPEG.h"                  // for BitPumpJPEG, BitStream<>:...
#include "io/ByteStream.h"                   // for ByteStream
#include <algorithm>                         // for min, transform
#include <array>                             // for array
#include <cassert>                           // for assert
#include <cstddef>                           // for size_t
#include <cstdint>                           // for uint16_t
#include <functional>                        // for cref, reference_wrapper
#include <initializer_list>                  // for initializer_list
#include <optional>                          // for optional
#include <tuple>                             // for make_tuple, operator==, get
#include <utility>                           // for move, index_sequence, mak...
#include <vector>                            // for vector

namespace rawspeed {

class ByteStream;

HasselbladDecompressor::HasselbladDecompressor(const RawImage& mRaw_,
                                               const PerComponentRecipe& rec_,
                                               ByteStream input_)
    : mRaw(mRaw_), rec(rec_), input(input_) {
  if (mRaw->getDataType() != RawImageType::UINT16)
    ThrowRDE("Unexpected data type");

  if (mRaw->getCpp() != 1 || mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected cpp: %u", mRaw->getCpp());

  // FIXME: could be wrong. max "active pixels" - "100 MP"
  if (mRaw->dim.x == 0 || mRaw->dim.y == 0 || mRaw->dim.x % 2 != 0 ||
      mRaw->dim.x > 12000 || mRaw->dim.y > 8816) {
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);
  }

  if (rec.ht.isFullDecode())
    ThrowRDE("Huffman table is of a full decoding variety");
}

// Returns len bits as a signed value.
// Highest bit is a sign bit
inline int HasselbladDecompressor::getBits(BitPumpMSB32& bs, int len) {
  if (!len)
    return 0;
  int diff = bs.getBits(len);
  diff = HuffmanTable<>::extend(diff, len);
  if (diff == 65535)
    return -32768;
  return diff;
}

ByteStream::size_type HasselbladDecompressor::decompress() {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  assert(out.height > 0);
  assert(out.width > 0);
  assert(out.width % 2 == 0);

  const auto ht = rec.ht;
  ht.verifyCodeSymbolsAreValidDiffLenghts();

  BitPumpMSB32 bitStream(input);
  // Pixels are packed two at a time, not like LJPEG:
  // [p1_length_as_huffman][p2_length_as_huffman][p0_diff_with_length][p1_diff_with_length]|NEXT
  // PIXELS
  for (int row = 0; row < out.height; row++) {
    int p1 = rec.initPred;
    int p2 = rec.initPred;
    for (int col = 0; col < out.width; col += 2) {
      int len1 = ht.decodeCodeValue(bitStream);
      int len2 = ht.decodeCodeValue(bitStream);
      p1 += getBits(bitStream, len1);
      p2 += getBits(bitStream, len2);
      // NOTE: this is rather unusual and weird, but appears to be correct.
      // clampBits(p, 16) results in completely garbled images.
      out(row, col) = uint16_t(p1);
      out(row, col + 1) = uint16_t(p2);
    }
  }
  return bitStream.getStreamPosition();
}

} // namespace rawspeed
