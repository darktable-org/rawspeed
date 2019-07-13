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
#include "common/Common.h"                // for uint32, uchar8, ushort16
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/BitPumpMSB.h"                // for BitPumpMSB
#include <cassert>                        // for assert

namespace rawspeed {

SonyArw1Decompressor::SonyArw1Decompressor(const RawImage& img) : mRaw(img) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  const uint32 w = mRaw->dim.x;
  const uint32 h = mRaw->dim.y;

  if (w == 0 || h == 0 || h % 2 != 0 || w > 4600 || h > 3072)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", w, h);
}

inline int SonyArw1Decompressor::getDiff(BitPumpMSB* bs, uint32 len) {
  if (len == 0)
    return 0;
  int diff = bs->getBitsNoFill(len);
  return HuffmanTable::extend(diff, len);
}

void SonyArw1Decompressor::decompress(const ByteStream& input) const {
  const uint32 w = mRaw->dim.x;
  const uint32 h = mRaw->dim.y;

  assert(w > 0);
  assert(h > 0);
  assert(h % 2 == 0);

  BitPumpMSB bits(input);
  uchar8* data = mRaw->getData();
  auto* dest = reinterpret_cast<ushort16*>(&data[0]);
  uint32 pitch = mRaw->pitch / sizeof(ushort16);
  int sum = 0;
  for (int64_t x = w - 1; x >= 0; x--) {
    for (uint32 y = 0; y < h + 1; y += 2) {
      bits.fill(32);

      if (y == h)
        y = 1;

      uint32 len = 4 - bits.getBitsNoFill(2);

      if (len == 3 && bits.getBitsNoFill(1))
        len = 0;

      if (len == 4)
        while (len < 17 && !bits.getBitsNoFill(1))
          len++;

      int diff = getDiff(&bits, len);
      sum += diff;

      if (sum < 0 || (sum >> 12) > 0)
        ThrowRDE("Error decompressing");

      if (y < h)
        dest[x + y * pitch] = sum;
    }
  }
}

} // namespace rawspeed
