#pragma once
#include "RawDecoderException.h"
#include "FileMap.h"
#include "BitPumpJPEG.h" // Includes bytestream
#include "RawImage.h"
#include "BitPumpMSB.h"
#include "BitPumpPlain.h"
#include "CameraMetaData.h"
/* 
    RawSpeed - RAW file decoder.

    Copyright (C) 2009 Klaus Post

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

    http://www.klauspost.com
*/
class RawDecoder 
{
public:
  RawDecoder(FileMap* file);
  virtual ~RawDecoder(void);
  virtual RawImage decodeRaw() = 0;
  virtual void checkSupport(CameraMetaData *meta) = 0;
  virtual void decodeMetaData(CameraMetaData *meta) = 0;
  FileMap *mFile; 
  void readUncompressedRaw(ByteStream &input, iPoint2D& size, iPoint2D& offset, int inputPitch, int bitPerPixel, gboolean MSBOrder);
  RawImage mRaw; 
  vector<const char*> errors;
protected:
  void checkCameraSupported(CameraMetaData *meta, string make, string model, string mode);
  virtual void setMetaData(CameraMetaData *meta, string make, string model, string mode);
  void Decode12BitRaw(ByteStream &input, guint w, guint h);
  void TrimSpaces( string& str);
};


