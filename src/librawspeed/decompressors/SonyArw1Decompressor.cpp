/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
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

#include "decompressors/SonyArw1Decompressor.h"
#include "adt/Array2DRef.h"
#include "adt/Bit.h"
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "adt/Point.h"
#include "bitstreams/BitStreamerMSB.h"
#include "codes/PrefixCodeDecoder.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "io/ByteStream.h"
#include <cstdint>
#include <utility>

namespace rawspeed {

SonyArw1Decompressor::SonyArw1Decompressor(RawImage img)
    : mRaw(std::move(img)) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  const uint32_t w = mRaw->dim.x;
  const uint32_t h = mRaw->dim.y;

  if (w == 0 || h == 0 || h % 2 != 0 || w > 4600 || h > 3072)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", w, h);
}

inline int SonyArw1Decompressor::getDiff(BitStreamerMSB& bs, uint32_t len) {
  if (len == 0)
    return 0;
  int diff = bs.getBitsNoFill(len);
  return PrefixCodeDecoder<>::extend(diff, len);
}

void SonyArw1Decompressor::decompress(ByteStream input) const {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  invariant(out.width() > 0);
  invariant(out.height() > 0);
  invariant(out.height() % 2 == 0);

  BitStreamerMSB bits(input.peekRemainingBuffer().getAsArray1DRef());
  int pred = 0;
  for (int col = out.width() - 1; col >= 0; col--) {
    for (int row = 0; row < out.height() + 1; row += 2) {
      bits.fill(32);

      if (row == out.height())
        row = 1;

      uint32_t len = 4 - bits.getBitsNoFill(2);

      if (len == 3 && bits.getBitsNoFill(1))
        len = 0;

      if (len == 4)
        while (len < 17 && !bits.getBitsNoFill(1))
          len++;

      int diff = getDiff(bits, len);
      pred += diff;

      if (!isIntN(pred, 12))
        ThrowRDE("Error decompressing");

      out(row, col) = implicit_cast<uint16_t>(pred);
    }
  }
}

} // namespace rawspeed
