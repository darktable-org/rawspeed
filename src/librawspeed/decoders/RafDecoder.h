/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2013 Klaus Post
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

#include "common/RawImage.h"     // for RawImage
#include "decoders/AbstractTiffDecoder.h"
#include "io/FileMap.h"          // for FileMap

namespace RawSpeed {

class CameraMetaData;

class RafDecoder final : public AbstractTiffDecoder
{
public:
  using AbstractTiffDecoder::AbstractTiffDecoder;

  RawImage decodeRawInternal() override;
  void decodeMetaDataInternal(CameraMetaData *meta) override;
  void checkSupportInternal(CameraMetaData *meta) override;
  static bool isRAF(FileMap* input);

protected:
  int getDecoderVersion() const override { return 1; }
  void decodeThreaded(RawDecoderThread *t) override;
  void DecodeRaf();
  bool alt_layout = false;
};

} // namespace RawSpeed
