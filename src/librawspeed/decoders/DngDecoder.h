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

#include "common/Common.h"                // for uint32
#include "common/RawImage.h"              // for RawImage
#include "decoders/AbstractTiffDecoder.h" // for AbstractTiffDecoder
#include "io/FileMap.h"                   // for FileMap
#include "tiff/TiffIFD.h"                 // for TiffIFD (ptr only), TiffRo...
#include <vector>                         // for vector

namespace RawSpeed {

class CameraMetaData;

class DngDecoder final : public AbstractTiffDecoder
{
public:
  DngDecoder(TiffRootIFDOwner&& rootIFD, FileMap* file);

  RawImage decodeRawInternal() override;
  void decodeMetaDataInternal(CameraMetaData *meta) override;
  void checkSupportInternal(CameraMetaData *meta) override;

private:
  uint32 sample_format{1};
  uint32 bps;

protected:
  int getDecoderVersion() const override { return 0; }
  bool mFixLjpeg;
  void dropUnsuportedChunks(std::vector<TiffIFD*>& data);
  void parseCFA(TiffIFD* raw);
  void decodeData(TiffIFD* raw, int compression);
  void printMetaData();
  bool decodeMaskedAreas(TiffIFD* raw);
  bool decodeBlackLevels(TiffIFD* raw);
  void setBlack(TiffIFD* raw);
};

} // namespace RawSpeed
