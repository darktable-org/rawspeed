/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2021 Daniel Vogelbacher

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

#pragma once

#include "common/RawImage.h"                    // for RawImage
#include "decoders/RawDecoderException.h"       // for ThrowRDE
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include <cassert>                              // for assert
#include <cstdint>                              // for uint16_t

namespace rawspeed {

class Buffer;
class RawImage;
class IsoMCanonCmp1Box;

class CrxDecompressor final : public AbstractDecompressor {
  RawImage mRaw;

public:
  CrxDecompressor(const RawImage& img);

  void decode(const IsoMCanonCmp1Box& cmp1Box, Buffer& crxRawData);

private:
  int crxDecodePlane(void* p, uint32_t planeNumber);
  void crxLoadDecodeLoop(void* img, int nPlanes);
  int crxParseImageHeader(uint8_t* cmp1TagData, int nTrack);
  void crxConvertPlaneLineDf(void* p, int imageRow);
  void crxLoadFinalizeLoopE3(void* p, int planeHeight);
};

} // namespace rawspeed
