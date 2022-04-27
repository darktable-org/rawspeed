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

#include "common/RawImage.h"              // for RawImage
#include "decoders/AbstractTiffDecoder.h" // for AbstractTiffDecoder
#include "decoders/RawDecoder.h"          // for RawDecoder::RawSlice
#include "tiff/TiffIFD.h"                 // for TiffIFD (ptr only), TiffRo...
#include <array>                          // for array
#include <cstdint>                        // for uint8_t, uint16_t, uint32_t
#include <string>                         // for string
#include <utility>                        // for move
#include <vector>                         // for vector

namespace rawspeed {

class ByteStream;
class CameraMetaData;
class iPoint2D;
class Buffer;

class NefDecoder final : public AbstractTiffDecoder
{
public:
  static bool isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                   const Buffer& file);
  NefDecoder(TiffRootIFDOwner&& root, const Buffer& file)
      : AbstractTiffDecoder(std::move(root), file) {}

  void decodeRawInternal() override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;
  void checkSupportInternal(const CameraMetaData* meta) override;

private:
  struct NefSlice final : RawSlice {};

  [[nodiscard]] int getDecoderVersion() const override { return 5; }
  [[nodiscard]] bool D100IsCompressed(uint32_t offset) const;
  static bool NEFIsUncompressed(const TiffIFD* raw);
  static bool NEFIsUncompressedRGB(const TiffIFD* raw);
  void DecodeUncompressed() const;
  void DecodeD100Uncompressed() const;
  void DecodeSNefUncompressed() const;
  void readCoolpixSplitRaw(ByteStream input, const iPoint2D& size,
                           const iPoint2D& offset, int inputPitch) const;
  void DecodeNikonSNef(const ByteStream& input) const;
  [[nodiscard]] std::string getMode() const;
  [[nodiscard]] std::string getExtendedMode(const std::string& mode) const;
  static std::vector<uint16_t> gammaCurve(double pwr, double ts, int mode,
                                          int imax);

  // We use this for the D50 and D2X whacky WB "encryption"
  static const std::array<uint8_t, 256> serialmap;
  static const std::array<uint8_t, 256> keymap;
};

} // namespace rawspeed
