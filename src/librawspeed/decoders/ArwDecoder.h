/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real

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
#include "common/RawImage.h"
#include "decoders/AbstractTiffDecoder.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "tiff/TiffIFD.h"
#include <cstdint>
#include <utility>
#include <vector>

namespace rawspeed {

class Buffer;
class CameraMetaData;

class ArwDecoder final : public AbstractTiffDecoder {
public:
  static bool isAppropriateDecoder(const TiffRootIFD* rootIFD, Buffer file);
  ArwDecoder(TiffRootIFDOwner&& root, Buffer file)
      : AbstractTiffDecoder(std::move(root), file) {}

  RawImage decodeRawInternal() override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;

private:
  void ParseA100WB() const;

  [[nodiscard]] int getDecoderVersion() const override { return 1; }
  static std::vector<uint16_t> decodeCurve(const TiffIFD* raw);
  RawImage decodeTransitionalArw();
  RawImage decodeSRF();
  void DecodeARW2(ByteStream input, uint32_t w, uint32_t h, uint32_t bpp);
  void DecodeLJpeg(const TiffIFD* raw);
  void DecodeUncompressed(const TiffIFD* raw) const;
  static void SonyDecrypt(Array1DRef<const uint8_t> ibuf,
                          Array1DRef<uint8_t> obuf, int len, uint32_t key);
  void GetWB() const;
  int mShiftDownScale = 0;
  int mShiftDownScaleForExif = 0;
};

} // namespace rawspeed
