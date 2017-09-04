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

#include "decompressors/SonyArw2Decompressor.h"
#include "common/Common.h"                                  // for uchar8
#include "common/Point.h"                                   // for iPoint2D
#include "common/RawImage.h"                                // for RawImage
#include "decompressors/AbstractParallelizedDecompressor.h" // for RawDecom...
#include "io/BitPumpLSB.h"                                  // for BitPumpLSB
#include <algorithm>                                        // for move

namespace rawspeed {

SonyArw2Decompressor::SonyArw2Decompressor(const RawImage& img,
                                           ByteStream input_)
    : AbstractParallelizedDecompressor(img), input(std::move(input_)) {
  // 1 byte per pixel
  input.check(mRaw->dim.x * mRaw->dim.y);
}

void SonyArw2Decompressor::decompressThreaded(
    const RawDecompressorThread* t) const {
  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  int32 w = mRaw->dim.x;

  BitPumpLSB bits(input);
  for (uint32 y = t->start; y < t->end; y++) {
    auto* dest = reinterpret_cast<ushort16*>(&data[y * pitch]);
    // Realign
    bits.setBufferPosition(w * y);
    uint32 random = bits.peekBits(24);

    // Process 32 pixels (16x2) per loop.
    for (int32 x = 0; x < w - 30;) {
      int _max = bits.getBits(11);
      int _min = bits.getBits(11);
      int _imax = bits.getBits(4);
      int _imin = bits.getBits(4);

      int sh = 0;
      while ((sh < 4) && ((0x80 << sh) <= (_max - _min)))
        sh++;

      for (int i = 0; i < 16; i++) {
        int p;
        if (i == _imax)
          p = _max;
        else {
          if (i == _imin)
            p = _min;
          else {
            p = (bits.getBits(7) << sh) + _min;
            if (p > 0x7ff)
              p = 0x7ff;
          }
        }
        mRaw->setWithLookUp(p << 1, reinterpret_cast<uchar8*>(&dest[x + i * 2]),
                            &random);
      }
      x += ((x & 1) != 0) ? 31 : 1; // Skip to next 32 pixels
    }
  }
}

} // namespace rawspeed
