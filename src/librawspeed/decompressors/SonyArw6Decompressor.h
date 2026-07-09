/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2026 Kabir Kwatra

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

#include "common/RawImage.h"
#include "decompressors/AbstractDecompressor.h"
#include "io/ByteStream.h"

namespace rawspeed {

// Decompressor for the Sony "ARW 6.0" format (the camera's "Compressed RAW 2"
// lossy mode, TIFF compression 32766), as used by the A7R VI (ILCE-7RM6) and
// A7 V (ILCE-7M5).
//
// The format is a wavelet codec: each spatial tile holds 3 visual components
// (Y full-res + Cb/Cr half-res), reconstructed via an inverse reversible 5/3
// DWT (LL0 + 3 detail levels), GCLI bitplane entropy coding, a log->linear LUT,
// and a YCC->RGGB mosaic compose. See SonyArw6Decompressor.cpp for the
// pipeline.
class SonyArw6Decompressor final : public AbstractDecompressor {
  RawImage mRaw;
  ByteStream input;

  // Width/height of the full active CFA.
  int width;
  int height;

public:
  SonyArw6Decompressor(RawImage img, ByteStream input, int width, int height);
  void decompress() const;
};

} // namespace rawspeed
