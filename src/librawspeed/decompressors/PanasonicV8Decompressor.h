/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2022-2024 LibRaw LLC (info@libraw.org)
    Copyright (C) 2024 Daniel Vogelbacher
    Copyright (C) 2025 Kolton Yager

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

#pragma once

#include "adt/Array1DRef.h"
#include "adt/Array1DRefExtras.h"
#include "adt/Array2DRef.h"
#include "adt/CroppedArray2DRef.h"
#include "codes/AbstractPrefixCode.h"
#include "codes/AbstractPrefixCodeDecoder.h"
#include "codes/PrefixCodeDecoder.h"
#include "codes/PrefixCodeVectorDecoder.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/AbstractDecompressor.h"
#include <array>
#include <cstdint>
#include <vector>

namespace rawspeed {

/// Decompressor for Panasonic's RW2 version 8 format.
///
/// V8 is similar to lossless JPEG compression from the JPEG 92 spec and DNG
/// Each raw file is broken up into a number of separate strips, each of which
/// was separately encoded, and which can be decoded independently. For each
/// strip, an initial predicted value is provided. The strip's data buffer is
/// then decoded using the Huffman table provided in metadata. Each value
/// decoded from the strip is a difference between the predicted value and
/// actual value, allowing the actual value to be reconstructed.
class PanasonicV8Decompressor final : public AbstractDecompressor {
private:
  mutable RawImage mRawOutput;

public:
  /// Four values, one for each component of the sensor's color filter array.
  using Bayer2x2 = std::array<uint16_t, 4>;

  using PrefixCodeDecoder =
      PrefixCodeLUTDecoder<BaselineCodeTag,
                           PrefixCodeVectorDecoder<BaselineCodeTag>>;

  /// Decompressor parameters populated from tags. They remain constant after
  /// construction.
  struct DecompressorParamsBuilder;

  struct DecompressorParams {
    const Array1DRef<const Array1DRef<const uint8_t>> mStrips;
    const Array1DRef<const iRectangle2D> mOutRect;

    const Bayer2x2 initialPrediction;

    DecompressorParams() = delete;

  private:
    friend struct DecompressorParamsBuilder;

    DecompressorParams(Array1DRef<const Array1DRef<const uint8_t>> mStrips_,
                       Array1DRef<const iRectangle2D> mOutRect_,
                       Bayer2x2 initialPrediction_)
        : mStrips(mStrips_), mOutRect(mOutRect_),
          initialPrediction(initialPrediction_) {}
  };

  struct DecompressorParamsBuilder {
    const Array1DRef<const Array1DRef<const uint8_t>> mStrips;
    const Bayer2x2 initialPrediction;

    const std::vector<iRectangle2D> mOutRects;

    std::vector<iRectangle2D> static getOutRects(
        iRectangle2D imgDim, Array1DRef<const uint32_t> stripLineOffsets,
        Array1DRef<const uint16_t> stripWidths,
        Array1DRef<const uint16_t> stripHeights);

    DecompressorParamsBuilder(
        iRectangle2D imgDim, Bayer2x2 initialPrediction_,
        Array1DRef<const Array1DRef<const uint8_t>> mStrips_,
        Array1DRef<const uint32_t> stripLineOffsets,
        Array1DRef<const uint16_t> stripWidths,
        Array1DRef<const uint16_t> stripHeights)
        : mStrips(mStrips_), initialPrediction(initialPrediction_),
          mOutRects(getOutRects(imgDim, stripLineOffsets, stripWidths,
                                stripHeights)) {
      if (mStrips.size() != implicit_cast<int>(mOutRects.size()))
        ThrowRDE("Got different number of input strips vs output tiles");
      for (const auto& strip : mStrips_) {
        if (strip.size() == 0)
          ThrowRDE("Got empty input strip");
      }
    }

    [[nodiscard]] DecompressorParams getDecompressorParams() const {
      return {mStrips, getAsArray1DRef(mOutRects), initialPrediction};
    }
  };

  // Pre-cached Huffman decoded values for rapid lookup.
  struct HuffmanLUTEntry {
    uint8_t bitcount = 7;
    uint8_t diffCat = 0;
  };

private:
  const DecompressorParams mParams;
  const PrefixCodeDecoder mCodeDecoder;

  /// Huffman decoder helper class. Defined only in the cpp file.
  class InternalHuffDecoder;

  /// Thread safe function for decompressing a single data-stripstrip within a
  /// Rw2V8 raw image.
  void decompressStrip(Array2DRef<uint16_t> out,
                       Array1DRef<const uint8_t> strip) const;

public:
  PanasonicV8Decompressor(RawImage outputImg, DecompressorParams mParams_,
                          PrefixCodeDecoder mCodeDecoder_);

  /// Run the decompressor on the provided raw image
  void decompress() const;
};

} // namespace rawspeed
