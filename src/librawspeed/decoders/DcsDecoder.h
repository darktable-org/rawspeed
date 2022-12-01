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

#include "common/RawImage.h"            // for RawImage
#include "decoders/SimpleTiffDecoder.h" // for SimpleTiffDecoder
#include "tiff/TiffIFD.h"               // for TiffRootIFD (ptr only), Tiff...
#include <utility>                      // for move

namespace rawspeed {

class Buffer;
class CameraMetaData;

class DcsDecoder final : public SimpleTiffDecoder {
  void checkImageDimensions() override;

public:
  static bool __attribute__((pure))
  isAppropriateDecoder(const TiffRootIFD* rootIFD, const Buffer& file);
  DcsDecoder(TiffRootIFDOwner&& root, const Buffer& file)
      : SimpleTiffDecoder(std::move(root), file) {}

  RawImage decodeRawInternal() override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;

private:
  [[nodiscard]] int getDecoderVersion() const override { return 0; }
};

} // namespace rawspeed
