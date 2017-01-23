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
#include "decoders/RawDecoder.h" // for RawDecoder
#include "io/FileMap.h"          // for FileMap
#include <string>                // for string

namespace RawSpeed {

class CameraMetaData;

class TiffIFD;

class MosDecoder :
  public RawDecoder
{
public:
  MosDecoder(TiffIFD *rootIFD, FileMap* file);
  virtual ~MosDecoder();
  virtual RawImage decodeRawInternal();
  virtual void checkSupportInternal(CameraMetaData *meta);
  virtual void decodeMetaDataInternal(CameraMetaData *meta);
protected:
  uint32 black_level;
  TiffIFD *mRootIFD;
  std::string make, model;
  std::string getXMPTag(const std::string &xmp, const std::string &tag);
  void DecodePhaseOneC(uint32 data_offset, uint32 strip_offset, uint32 width, uint32 height);
};

} // namespace RawSpeed
