/*
    RawSpeed - RAW file decoder.

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

#include "decompressors/LJpegDecompressor.h"
#include "common/Common.h"
#include "io/BitPumpJPEG.h"
#include "io/ByteStream.h"

using namespace std;

namespace RawSpeed {

void LJpegDecompressor::decode(uint32 offsetX, uint32 offsetY, bool fixDng16Bug_) {
  if ((int)offsetX >= mRaw->dim.x)
    ThrowRDE("LJpegDecompressor: X offset outside of image");
  if ((int)offsetY >= mRaw->dim.y)
    ThrowRDE("LJpegDecompressor: Y offset outside of image");
  offX = offsetX;
  offY = offsetY;

  fixDng16Bug = fixDng16Bug_;

  AbstractLJpegDecompressor::decode();
}

void LJpegDecompressor::decodeScan()
{
  if (predictorMode != 1)
    ThrowRDE("LJpegDecompressor: Unsupported predictor mode");

  for (uint32 i = 0; i < frame.cps;  i++)
    if (frame.compInfo[i].superH != 1 || frame.compInfo[i].superV != 1)
      ThrowRDE("LJpegDecompressor: Unsupported subsampling");

  if (frame.cps == 2)
    decodeN<2>();
  else if (frame.cps == 3)
    decodeN<3>();
  else if (frame.cps == 4)
    decodeN<4>();
  else
    ThrowRDE("LJpegDecompressor: Unsupported number of components");
}

// N_COMP == number of components (2, 3 or 4)

template <int N_COMP>
void LJpegDecompressor::decodeN()
{
  auto ht = getHuffmanTables<N_COMP>();
  auto pred = getInitialPredictors<N_COMP>();
  auto predNext = pred.data();

  BitPumpJPEG bitStream(input);

  for (unsigned y = 0; y < frame.h; ++y) {
    auto destY = offY + y;
    // A recoded DNG might be split up into tiles of self contained LJpeg
    // blobs. The tiles at the bottom and the right may extend beyond the
    // dimension of the raw image buffer. The excessive content has to be
    // ignored. For y, we can simply stop decoding when we reached the border.
    if (destY >= (unsigned)mRaw->dim.y)
      break;

    auto dest = (ushort16*)mRaw->getDataUncropped(offX, destY);

    copy_n(predNext, N_COMP, pred.data());
    // the predictor for the next line is the start of this line
    predNext = dest;

    unsigned width = min(frame.w,
                         (mRaw->dim.x - offX) / (N_COMP / mRaw->getCpp()));

    // For x, we first process all pixels within the image buffer ...
    for (unsigned x = 0; x < width; ++x) {
      unroll_loop<N_COMP>([&](int i) {
        *dest++ = pred[i] += ht[i]->decodeNext(bitStream);
      });
    }
    // ... and discard the rest.
    for (unsigned x = width; x < frame.w; ++x) {
      unroll_loop<N_COMP>([&](int i) {
        ht[i]->decodeNext(bitStream);
      });
    }
  }
  input.skipBytes(bitStream.getBufferPosition());
}

} // namespace RawSpeed
