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

#include "common/Common.h"       // for uint32
#include "common/RawImage.h"     // for RawImage
#include "decoders/RawDecoder.h" // for RawDecoder, RawDecoderThread (ptr o...
#include "io/FileMap.h"          // for FileMap

namespace RawSpeed {

class ByteStream;
class CameraMetaData;
class TiffIFD;

class ArwDecoder :
  public RawDecoder
{
public:
  ArwDecoder(TiffIFD *rootIFD, FileMap* file);
  ~ArwDecoder() override;
  RawImage decodeRawInternal() override;
  void checkSupportInternal(CameraMetaData *meta) override;
  void decodeMetaDataInternal(CameraMetaData *meta) override;
  void decodeThreaded(RawDecoderThread *t) override;
  TiffIFD *getRootIFD() override { return mRootIFD; }

protected:
  int getDecoderVersion() const override { return 1; }
  void DecodeARW(ByteStream &input, uint32 w, uint32 h);
  void DecodeARW2(ByteStream &input, uint32 w, uint32 h, uint32 bpp);
  void DecodeUncompressed(TiffIFD* raw);
  void SonyDecrypt(uint32 *buffer, uint32 len, uint32 key);
  void GetWB();
  TiffIFD *mRootIFD;
  ByteStream *in;
  int mShiftDownScale;
};

} // namespace RawSpeed
