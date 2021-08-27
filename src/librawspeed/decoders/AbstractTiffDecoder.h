/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser

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

#include "decoders/RawDecoder.h" // for RawDecoder
#include "tiff/TiffIFD.h"        // for TiffID, TiffRootIFD, TiffRootIFDOwner
#include "tiff/TiffTag.h"        // for IMAGEWIDTH, TiffTag
#include <memory>                // for unique_ptr
#include <string>                // for string
#include <utility>               // for move

namespace rawspeed {

class CameraMetaData;

class Buffer;

class AbstractTiffDecoder : public RawDecoder
{
protected:
  TiffRootIFDOwner mRootIFD;
public:
  AbstractTiffDecoder(TiffRootIFDOwner&& root, const Buffer& file)
      : RawDecoder(file), mRootIFD(std::move(root)) {}

  TiffIFD* getRootIFD() final { return mRootIFD.get(); }

  inline bool checkCameraSupported(const CameraMetaData* meta, const TiffID& id,
                                   const std::string& mode) {
    return RawDecoder::checkCameraSupported(meta, id.make, id.model, mode);
  }

  using RawDecoder::setMetaData;

  inline void setMetaData(const CameraMetaData* meta, const TiffID& id,
                          const std::string& mode, int iso_speed) {
    setMetaData(meta, id.make, id.model, mode, iso_speed);
  }

  inline void setMetaData(const CameraMetaData* meta, const std::string& mode,
                          int iso_speed) {
    setMetaData(meta, mRootIFD->getID(), mode, iso_speed);
  }

  inline void checkSupportInternal(const CameraMetaData* meta) override {
    checkCameraSupported(meta, mRootIFD->getID(), "");
  }

  [[nodiscard]] const TiffIFD*
  getIFDWithLargestImage(TiffTag filter = IMAGEWIDTH) const;
};

} // namespace rawspeed
