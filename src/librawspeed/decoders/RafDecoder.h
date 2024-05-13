/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2013 Klaus Post
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

#include "adt/Point.h"
#include "common/RawImage.h"
#include "decoders/AbstractTiffDecoder.h"
#include "io/Buffer.h"
#include "tiff/TiffIFD.h"
#include <utility>

namespace rawspeed {

class Buffer;
class Camera;
class CameraMetaData;

class RafDecoder final : public AbstractTiffDecoder {
  bool alt_layout = false;

public:
  static bool isAppropriateDecoder(const TiffRootIFD* rootIFD, Buffer file);
  RafDecoder(TiffRootIFDOwner&& root, Buffer file)
      : AbstractTiffDecoder(std::move(root), file) {}

  RawImage decodeRawInternal() override;
  void applyCorrections(const Camera* cam);
  void decodeMetaDataInternal(const CameraMetaData* meta) override;
  void checkSupportInternal(const CameraMetaData* meta) override;
  iRectangle2D getDefaultCrop() override;
  static bool isRAF(Buffer input);

protected:
  [[nodiscard]] int getDecoderVersion() const override { return 1; }

private:
  [[nodiscard]] int isCompressed() const;
};

} // namespace rawspeed
