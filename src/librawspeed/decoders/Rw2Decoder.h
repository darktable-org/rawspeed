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

#include "adt/Point.h"
#include "common/RawImage.h"
#include "decoders/AbstractTiffDecoder.h"
#include "io/Buffer.h"
#include "tiff/TiffIFD.h"
#include <string>
#include <utility>

namespace rawspeed {

class Buffer;
class CameraMetaData;

class Rw2Decoder final : public AbstractTiffDecoder {
public:
  static bool isAppropriateDecoder(const TiffRootIFD* rootIFD, Buffer file);
  Rw2Decoder(TiffRootIFDOwner&& root, Buffer file)
      : AbstractTiffDecoder(std::move(root), file) {}

  RawImage decodeRawInternal() override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;
  void checkSupportInternal(const CameraMetaData* meta) override;
  iRectangle2D getDefaultCrop() override;

protected:
  [[nodiscard]] int getDecoderVersion() const override { return 3; }

private:
  void parseCFA() const;
  [[nodiscard]] const TiffIFD* getRaw() const;
  [[nodiscard]] std::string guessMode() const;
};

} // namespace rawspeed
