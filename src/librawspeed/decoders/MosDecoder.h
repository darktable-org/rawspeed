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

#include "common/RawImage.h"              // for RawImage
#include "decoders/AbstractTiffDecoder.h" // for AbstractTiffDecoder
#include "tiff/TiffIFD.h"                 // for TiffRootIFD (ptr only)
#include <string>                         // for string

namespace rawspeed {

class CameraMetaData;
class Buffer;

class MosDecoder final : public AbstractTiffDecoder
{
public:
  static bool isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                   const Buffer* file);
  MosDecoder(TiffRootIFDOwner&& rootIFD, const Buffer* file);

  RawImage decodeRawInternal() override;
  void checkSupportInternal(const CameraMetaData* meta) override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;

protected:
  int getDecoderVersion() const override { return 0; }
  std::string make, model;
  static std::string getXMPTag(const std::string& xmp, const std::string& tag);
};

} // namespace rawspeed
