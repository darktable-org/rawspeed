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

#include "common/Common.h"       // for uint32
#include "common/RawImage.h"     // for RawImage
#include "decoders/RawDecoder.h" // for RawDecoder
#include "io/FileMap.h"          // for FileMap

namespace RawSpeed {

class CameraMetaData;
class TiffIFD;

class DngDecoder :
  public RawDecoder
{
public:
  DngDecoder(TiffIFD *rootIFD, FileMap* file);
  virtual ~DngDecoder();
  virtual RawImage decodeRawInternal();
  virtual void decodeMetaDataInternal(CameraMetaData *meta);
  virtual void checkSupportInternal(CameraMetaData *meta);
  virtual TiffIFD* getRootIFD() {return mRootIFD;}
protected:
  TiffIFD *mRootIFD;
  bool mFixLjpeg;
  void printMetaData();
  bool decodeMaskedAreas(TiffIFD* raw);
  bool decodeBlackLevels(TiffIFD* raw);
  void setBlack(TiffIFD* raw);
};

class DngStrip {
public:
  DngStrip() { h = offset = count = offsetY = 0;};
  ~DngStrip() = default;
  uint32 h;
  uint32 offset; // Offset in bytes
  uint32 count;
  uint32 offsetY;
};

} // namespace RawSpeed
