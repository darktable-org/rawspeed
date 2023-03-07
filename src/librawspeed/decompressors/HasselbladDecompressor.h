/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2023 Roman Lebedev

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

#include "adt/Point.h"                    // for iPoint2D
#include "adt/iterator_range.h"           // for iterator_range
#include "common/RawImage.h"              // for RawImage
#include "common/RawspeedException.h"     // for ThrowException
#include "decoders/RawDecoderException.h" // for ThrowException, ThrowRDE
#include "decompressors/HuffmanTable.h"   // for HuffmanTable, HuffmanTa...
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32
#include "io/ByteStream.h"                // for ByteStream
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstddef>                        // for size_t, ptrdiff_t
#include <cstdint>                        // for uint16_t
#include <functional>                     // for reference_wrapper
#include <iterator>                       // for input_iterator_tag
#include <tuple>                          // for tuple
#include <utility>                        // for index_sequence
#include <vector>                         // for vector

namespace rawspeed {

class HasselbladDecompressor final {
public:
  struct PerComponentRecipe {
    const HuffmanTable<>& ht;
    const uint16_t initPred;
  };

private:
  const RawImage mRaw;

  const PerComponentRecipe& rec;

  const ByteStream input;

  static int getBits(BitPumpMSB32& bs, int len);

public:
  HasselbladDecompressor(const RawImage& mRaw, const PerComponentRecipe& rec,
                         ByteStream input);

  [[nodiscard]] ByteStream::size_type decompress();
};

} // namespace rawspeed
