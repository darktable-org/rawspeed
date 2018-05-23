/*
    RawSpeed - RAW file decoder.

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

#pragma once

#include "common/Common.h"                                  // for uint32
#include "decompressors/AbstractParallelizedDecompressor.h" // for Abstract...
#include "io/ByteStream.h"                                  // for ByteStream

namespace rawspeed {

class RawImage;

class PanasonicDecompressorV5 final : public AbstractParallelizedDecompressor {
  static constexpr uint32 SerializationBlockSize = 0x4000;
  static constexpr uint32 SerializationBlockSizeMask =
      0x3FFF; // == 0x4000 - 1, set all bits
  static constexpr uint32 PixelDataBlockSize = 16;

  struct DataPump;

  void decompressThreaded(const RawDecompressorThread* t) const final;

  ByteStream input;
  const bool zero_is_bad;

  // The RW2 raw image buffer is split into sections of BufSize bytes.
  // If section_split_offset is 0, then the last section is not neccesarily
  // full. If section_split_offset is not 0, then each section has two parts:
  //     bytes: [0..section_split_offset-1][section_split_offset..BufSize-1]
  //     pixels: [a..b][0..a-1]
  //   I.e. these two parts need to be swapped around.
  const uint32 section_split_offset; // FIXME there is no point in passing this
                                     // via a constructor; make private constant

  const uint32 bps;
  const uint32 encodedDataSize; // computed in constructor

public:
  PanasonicDecompressorV5(const RawImage& img, const ByteStream& input_,
                          bool zero_is_not_bad, uint32 section_split_offset_,
                          uint32 bps_);
};

} // namespace rawspeed
