/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2026 Nikolay Amiantov

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

#include "adt/Array1DRef.h"
#include "adt/Point.h"
#include "common/RawImage.h"
#include "decompressors/AbstractDecompressor.h"
#include "io/ByteStream.h"
#include <cstdint>
#include <vector>

namespace rawspeed {

// Decompressor for Sony's lossy compressed raws ("ARW6", TIFF compression
// 32766).
class SonyArw6Decompressor final : public AbstractDecompressor {
  struct TileDesc final {
    ByteStream bs; // the tile's compressed payload
    iPoint2D pos;  // top-left corner in the mosaic
    iPoint2D dim;  // size in mosaic pixels
  };

  RawImage mRaw;
  std::vector<TileDesc> tiles;
  bool applyCurve;

  void decompressTile(const TileDesc& tile) const;
  void decompressThread() const noexcept;

public:
  // `input` is the raw strip. If `applyCurve_` is unset the decoded 12-bit
  // codes are output as-is, without the final delinearization step.
  SonyArw6Decompressor(RawImage img, ByteStream input, bool applyCurve_);

  void decompress() const;

  // The codec's fixed curve mapping each decoded 12-bit code to a linear
  // sensor value.
  static Array1DRef<const uint16_t> delinearizationCurve();
};

} // namespace rawspeed
