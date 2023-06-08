/*
    RawSpeed - RAW file decoder.

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

#include "adt/Point.h"               // for iPoint2D, iRectangle2D
#include "codes/PrefixCodeDecoder.h" // for PrefixCodeDecoder
#include "common/RawImage.h"         // for RawImage
#include "io/ByteStream.h"           // for ByteStream
#include <array>                     // for array
#include <cstdint>                   // for uint16_t
#include <functional>                // for reference_wrapper
#include <stddef.h>                  // for size_t
#include <tuple>                     // for array
#include <vector>                    // for vector

namespace rawspeed {

// Decompresses Lossless JPEGs, with 2-4 components

class LJpegDecompressor final {
public:
  struct Frame {
    const int cps;
    const iPoint2D dim;
  };
  struct PerComponentRecipe {
    const PrefixCodeDecoder<>& ht;
    const uint16_t initPred;
  };

private:
  RawImage mRaw;
  ByteStream input;

  const iRectangle2D imgFrame;

  const Frame frame;
  const std::vector<PerComponentRecipe> rec;

  const iPoint2D MCUSize;

  int fullCols = 0;
  bool havePartialCol = false;
  bool interleaveRows = false;

  template <int N_COMP, size_t... I>
  [[nodiscard]] std::array<std::reference_wrapper<const PrefixCodeDecoder<>>,
                           N_COMP>
      getPrefixCodeDecodersImpl(std::index_sequence<I...> /*unused*/) const;

  template <int N_COMP>
  [[nodiscard]] std::array<std::reference_wrapper<const PrefixCodeDecoder<>>,
                           N_COMP>
  getPrefixCodeDecoders() const;

  template <int N_COMP>
  [[nodiscard]] std::array<uint16_t, N_COMP> getInitialPreds() const;

  template <const iPoint2D& MCUSize> void decodeN();

public:
  LJpegDecompressor(const RawImage& img, iRectangle2D imgFrame, Frame frame,
                    std::vector<PerComponentRecipe> rec, ByteStream bs,
                    bool interleaveRows = false);

  void decode();
};

} // namespace rawspeed
