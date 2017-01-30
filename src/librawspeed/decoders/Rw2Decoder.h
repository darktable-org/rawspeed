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

#include "common/Common.h"       // for uint32, uchar8
#include "common/RawImage.h"     // for RawImage
#include "decoders/AbstractTiffDecoder.h"
#include "io/FileMap.h"          // for FileMap
#include <string>                // for string

namespace RawSpeed {

class ByteStream;

class CameraMetaData;

class PanaBitpump {
  public:
  PanaBitpump(ByteStream* input);
  virtual ~PanaBitpump();
  ByteStream* input;
  uchar8* buf;
  int vbits;
  uint32 load_flags;
  uint32 getBits(int nbits);
  void skipBytes(int bytes);
};

class Rw2Decoder final : public AbstractTiffDecoder
{
public:
  using AbstractTiffDecoder::AbstractTiffDecoder;
  ~Rw2Decoder() override;

  RawImage decodeRawInternal() override;
  void decodeMetaDataInternal(CameraMetaData *meta) override;
  void checkSupportInternal(CameraMetaData *meta) override;

protected:
  int getDecoderVersion() const override { return 2; }
  void decodeThreaded(RawDecoderThread *t) override;

private:
  void DecodeRw2();
  std::string guessMode();
  ByteStream* input_start = nullptr;
  uint32 load_flags;
};

} // namespace RawSpeed
