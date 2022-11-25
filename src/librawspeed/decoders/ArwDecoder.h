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

#include "common/RawImage.h"              // for RawImage
#include "decoders/AbstractTiffDecoder.h" // for AbstractTiffDecoder
#include "io/ByteStream.h"                // for ByteStream
#include "tiff/TiffIFD.h"                 // for TiffIFD (ptr only), TiffRo...
#include <cstdint>                        // for uint32_t
#include <utility>                        // for move

namespace rawspeed {

class Buffer;
class CameraMetaData;

class ArwDecoder final : public AbstractTiffDecoder
{
public:
  static bool isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                   const Buffer& file);
  ArwDecoder(TiffRootIFDOwner&& root, const Buffer& file)
      : AbstractTiffDecoder(std::move(root), file) {}

  RawImage decodeRawInternal() override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;

private:
  void ParseA100WB() const;

  [[nodiscard]] int getDecoderVersion() const override { return 1; }
  RawImage decodeSRF(const TiffIFD* raw);
  void DecodeARW2(const ByteStream& input, uint32_t w, uint32_t h,
                  uint32_t bpp);
  void DecodeUncompressed(const TiffIFD* raw) const;
  static void SonyDecrypt(const uint32_t* ibuf, uint32_t* obuf, uint32_t len,
                          uint32_t key);
  void GetWB() const;
  ByteStream in;
  int mShiftDownScale = 0;
  int mShiftDownScaleForExif = 0;
};

} // namespace rawspeed
