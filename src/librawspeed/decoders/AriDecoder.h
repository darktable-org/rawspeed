/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2015 Klaus Post

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

#include "common/Common.h"       // for uint32
#include "common/RawImage.h"     // for RawImage
#include "decoders/RawDecoder.h" // for RawDecoder, RawDecoderThread (ptr o...
#include <string>                // for string

namespace rawspeed {

class CameraMetaData;

class Buffer;

class AriDecoder final : public RawDecoder {
public:
  explicit AriDecoder(Buffer* file);
  RawImage decodeRawInternal() override;
  void checkSupportInternal(const CameraMetaData* meta) override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;
  void decodeThreaded(RawDecoderThread *t) override;
  static bool isARI(Buffer* input);

protected:
  int getDecoderVersion() const override { return 0; }
  uint32 mWidth, mHeight, mIso;
  std::string mModel;
  std::string mEncoder;
  uint32 mDataOffset, mDataSize;
  float mWB[3];
};

} // namespace rawspeed
