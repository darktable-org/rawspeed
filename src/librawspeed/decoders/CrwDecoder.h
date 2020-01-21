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

#include "common/RawImage.h"     // for RawImage
#include "decoders/RawDecoder.h" // for RawDecoder
#include "tiff/CiffIFD.h"        // for CiffIFD
#include <cstdint>               // for int64_t
#include <memory>                // for unique_ptr

namespace rawspeed {

class Buffer;

class CameraMetaData;

class CrwDecoder final : public RawDecoder {
  std::unique_ptr<const CiffIFD> mRootIFD;

public:
  CrwDecoder(std::unique_ptr<const CiffIFD> rootIFD, const Buffer* file);
  RawImage decodeRawInternal() override;
  void checkSupportInternal(const CameraMetaData* meta) override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;
  static bool isCRW(const Buffer* input);

protected:
  int getDecoderVersion() const override { return 0; }
  static float canonEv(int64_t in);
};

} // namespace rawspeed
