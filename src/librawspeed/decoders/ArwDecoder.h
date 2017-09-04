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

#include "common/Common.h"                // for uint32
#include "common/RawImage.h"              // for RawImage
#include "decoders/AbstractTiffDecoder.h" // for AbstractTiffDecoder
#include "io/ByteStream.h"                // for ByteStream
#include "tiff/TiffIFD.h"                 // for TiffIFD (ptr only), TiffRo...
#include <algorithm>                      // for move

namespace rawspeed {

class CameraMetaData;

class Buffer;

class ArwDecoder final : public AbstractTiffDecoder
{
public:
  static bool isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                   const Buffer* file);
  ArwDecoder(TiffRootIFDOwner&& root, const Buffer* file)
      : AbstractTiffDecoder(move(root), file) {}

  RawImage decodeRawInternal() override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;

protected:
  void ParseA100WB();

  int getDecoderVersion() const override { return 1; }
  RawImage decodeSRF(const TiffIFD* raw);
  void DecodeARW(const ByteStream& input, uint32 w, uint32 h);
  void DecodeARW2(const ByteStream& input, uint32 w, uint32 h, uint32 bpp);
  void DecodeUncompressed(const TiffIFD* raw);
  void SonyDecrypt(const uint32* ibuf, uint32* obuf, uint32 len, uint32 key);
  void GetWB();
  ByteStream in;
  int mShiftDownScale = 0;
};

} // namespace rawspeed
