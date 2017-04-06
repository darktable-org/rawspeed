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

#include "common/Common.h"                // for int32
#include "common/RawImage.h"              // for RawImage
#include "decoders/AbstractTiffDecoder.h" // for AbstractTiffDecoder
#include "io/BitPumpMSB.h"                // for BitPumpMSB
#include "tiff/TiffIFD.h"                 // for TiffIFD (ptr only), TiffRo...
#include <algorithm>                      // for move
#include <string>                         // for string
#include <vector>                         // for vector

namespace rawspeed {

class CameraMetaData;
class Buffer;

class SrwDecoder final : public AbstractTiffDecoder
{
public:
  // please revert _this_ commit, once IWYU can handle inheriting constructors
  // using AbstractTiffDecoder::AbstractTiffDecoder;
  SrwDecoder(TiffRootIFDOwner&& root, Buffer* file)
      : AbstractTiffDecoder(move(root), file) {}

  RawImage decodeRawInternal() override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;
  void checkSupportInternal(const CameraMetaData* meta) override;

private:
  struct encTableItem;

  int getDecoderVersion() const override { return 3; }
  void decodeCompressed(const TiffIFD* raw);
  void decodeCompressed2(const TiffIFD* raw, int bits);
  void decodeCompressed3(const TiffIFD* raw, int bits);
  std::string getMode();
  static int32 samsungDiff(BitPumpMSB& pump,
                           const std::vector<encTableItem>& tbl);
};

} // namespace rawspeed
