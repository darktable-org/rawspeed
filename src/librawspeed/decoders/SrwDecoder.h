/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2010 Klaus Post

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

#include "common/Common.h"       // for uchar8, int32
#include "common/RawImage.h"     // for RawImage
#include "decoders/RawDecoder.h" // for RawDecoder
#include "io/BitPumpMSB.h"       // for BitPumpMSB
#include "io/FileMap.h"          // for FileMap
#include <string>                // for string

namespace RawSpeed {

class CameraMetaData;

class TiffIFD;

class SrwDecoder :
  public RawDecoder
{
public:
  SrwDecoder(TiffIFD *rootIFD, FileMap* file);
  ~SrwDecoder() override;
  RawImage decodeRawInternal() override;
  void decodeMetaDataInternal(CameraMetaData *meta) override;
  void checkSupportInternal(CameraMetaData *meta) override;
  TiffIFD *getRootIFD() override { return mRootIFD; }

private:
  struct encTableItem {
    uchar8 encLen;
    uchar8 diffLen;
  };

  int getDecoderVersion() const override { return 3; }
  void decodeCompressed(TiffIFD* raw);
  void decodeCompressed2(TiffIFD* raw, int bits);
  int32 samsungDiff (BitPumpMSB &pump, encTableItem *tbl);
  void decodeCompressed3(TiffIFD* raw, int bits);
  std::string getMode();
  TiffIFD *mRootIFD;
};

} // namespace RawSpeed
