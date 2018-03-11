/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "common/Common.h"                                  // for uint32
#include "decompressors/AbstractParallelizedDecompressor.h" // for Abstract...
#include "io/ByteStream.h"                                  // for ByteStream
#include <cassert>                                          // for assert
#include <utility>                                          // for move
#include <vector>                                           // for vector

namespace rawspeed {

class RawImage;

struct DngTilingDescription final {
  // The dimensions of the whole image.
  const iPoint2D& dim;

  // How many horizontal pixels does one tile represent?
  const uint32 tileW;

  // How many vertical pixels does one tile represent?
  const uint32 tileH;

  // How many tiles per row is there?
  const uint32 tilesX;

  // How many rows is there?
  const uint32 tilesY;

  // How many tiles are there total?
  const unsigned numTiles;

  DngTilingDescription(const iPoint2D& dim_, uint32 tileW_, uint32 tileH_)
      : dim(dim_), tileW(tileW_), tileH(tileH_),
        tilesX(roundUpDivision(dim.x, tileW)),
        tilesY(roundUpDivision(dim.y, tileH)), numTiles(tilesX * tilesY) {
    assert(dim.area() > 0);
    assert(tileW > 0);
    assert(tileH > 0);
    assert(tilesX > 0);
    assert(tilesY > 0);
    assert(tileW * tilesX >= static_cast<unsigned>(dim.x));
    assert(tileH * tilesY >= static_cast<unsigned>(dim.y));
    assert(tileW * (tilesX - 1) < static_cast<unsigned>(dim.x));
    assert(tileH * (tilesY - 1) < static_cast<unsigned>(dim.y));
    assert(numTiles > 0);
  }
};

struct DngSliceElement final {
  const DngTilingDescription& dsc;

  // Which slice is this?
  const unsigned n;

  // The actual data of the tile.
  const ByteStream bs;

  // Which tile is this?
  const unsigned column;
  const unsigned row;

  const bool lastColumn;
  const bool lastRow;

  // Where does it start?
  const unsigned offX;
  const unsigned offY;

  // What's it's actual size?
  const unsigned width;
  const unsigned height;

  DngSliceElement(const DngTilingDescription& dsc_, unsigned n_, ByteStream bs_)
      : dsc(dsc_), n(n_), bs(std::move(bs_)), column(n % dsc.tilesX),
        row(n / dsc.tilesX), lastColumn((column + 1) == dsc.tilesX),
        lastRow((row + 1) == dsc.tilesY), offX(dsc.tileW * column),
        offY(dsc.tileH * row),
        width(!lastColumn ? dsc.tileW : dsc.dim.x - offX),
        height(!lastRow ? dsc.tileH : dsc.dim.y - offY) {
    assert(n < dsc.numTiles);
    assert(bs.getRemainSize() > 0);
    assert(column < dsc.tilesX);
    assert(row < dsc.tilesY);
    assert(offX < static_cast<unsigned>(dsc.dim.x));
    assert(offY < static_cast<unsigned>(dsc.dim.y));
    assert(width > 0);
    assert(height > 0);
    assert(offX + width <= static_cast<unsigned>(dsc.dim.x));
    assert(offY + height <= static_cast<unsigned>(dsc.dim.y));
    assert(!lastColumn || (offX + width == static_cast<unsigned>(dsc.dim.x)));
    assert(!lastRow || (offY + height == static_cast<unsigned>(dsc.dim.y)));
  }
};

class AbstractDngDecompressor final : public AbstractParallelizedDecompressor {
  void decompressThreaded(const RawDecompressorThread* t) const final;

public:
  AbstractDngDecompressor(const RawImage& img, DngTilingDescription dsc_,
                          int compression_, bool mFixLjpeg_, uint32 mBps_,
                          uint32 mPredictor_)
      : AbstractParallelizedDecompressor(img), dsc(dsc_),
        compression(compression_), mFixLjpeg(mFixLjpeg_), mBps(mBps_),
        mPredictor(mPredictor_) {}

  void decompress() const final;

  const DngTilingDescription dsc;

  std::vector<DngSliceElement> slices;

  const int compression;
  const bool mFixLjpeg = false;
  const uint32 mBps;
  const uint32 mPredictor;
};

} // namespace rawspeed
