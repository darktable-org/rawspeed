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

#include "decompressors/HasselbladLJpegDecoder.h"
#include "adt/Array2DRef.h"               // for Array2DRef
#include "adt/Point.h"                    // for iPoint2D
#include "common/Common.h"                // for to_array
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowException, ThrowRDE
#include "decompressors/HasselbladDecompressor.h" // for HasselbladDecompressor
#include "decompressors/HuffmanTable.h" // for HuffmanTableLUT, HuffmanTable
#include "io/BitPumpMSB32.h"            // for BitPumpMSB32, BitStream<>:...
#include "io/ByteStream.h"              // for ByteStream
#include <array>                        // for array
#include <cassert>                      // for assert
#include <cstdint>                      // for uint16_t

namespace rawspeed {

HasselbladLJpegDecoder::HasselbladLJpegDecoder(ByteStream bs,
                                               const RawImage& img)
    : AbstractLJpegDecoder(bs, img) {
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

void HasselbladLJpegDecoder::decodeScan() {
  if (frame.w != static_cast<unsigned>(mRaw->dim.x) ||
      frame.h != static_cast<unsigned>(mRaw->dim.y)) {
    ThrowRDE("LJPEG frame does not match EXIF dimensions: (%u; %u) vs (%i; %i)",
             frame.w, frame.h, mRaw->dim.x, mRaw->dim.y);
  }

  const HasselbladDecompressor::PerComponentRecipe rec = {
      *getHuffmanTables(1)[0], getInitialPredictors(1)[0]};

  HasselbladDecompressor d(mRaw, rec, input);
  input.skipBytes(d.decompress());
}

void HasselbladLJpegDecoder::decode() {
  // We cannot use fully decoding huffman table,
  // because values are packed two pixels at the time.
  fullDecodeHT = false;

  AbstractLJpegDecoder::decodeSOI();
}

} // namespace rawspeed
