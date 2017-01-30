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

#include "common/Common.h"       // for uint32, ushort16
#include "common/RawImage.h"     // for RawImage
#include "decoders/AbstractTiffDecoder.h"
#include "io/FileMap.h"          // for FileMap

namespace RawSpeed {

class ByteStream;

class CameraMetaData;

class DcrDecoder final : public AbstractTiffDecoder
{
public:
  using AbstractTiffDecoder::AbstractTiffDecoder;

  RawImage decodeRawInternal() override;
  void checkSupportInternal(CameraMetaData *meta) override;
  void decodeMetaDataInternal(CameraMetaData *meta) override;

protected:
  int getDecoderVersion() const override { return 0; }
  void decodeKodak65000(ByteStream &input, uint32 w, uint32 h);
  void decodeKodak65000Segment(ByteStream &input, ushort16 *out, uint32 bsize);
};

} // namespace RawSpeed
