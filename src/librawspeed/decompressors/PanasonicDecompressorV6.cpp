/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2020 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "decompressors/PanasonicDecompressorV6.h" // for PanasonicDecompre...
#include "common/RawImage.h"                       // for RawImage, RawImag...
#include "decoders/RawDecoderException.h"          // for ThrowRDE
#include <cstdint>                                 // for uint16_t, uint32_t

namespace rawspeed {

class ByteStream;

PanasonicDecompressorV6::PanasonicDecompressorV6(const RawImage& img,
                                                 const ByteStream& input_,
                                                 uint32_t bps_)
    : mRaw(img) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");
}

void PanasonicDecompressorV6::decompress() const { (void)mRaw; }

} // namespace rawspeed
