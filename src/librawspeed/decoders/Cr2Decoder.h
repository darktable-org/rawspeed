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

#include "common/RawImage.h"              // for RawImage
#include "decoders/AbstractTiffDecoder.h" // for AbstractTiffDecoder
#include "io/FileMap.h"                   // for FileMap
#include "tiff/TiffIFD.h"                 // for TiffRootIFDOwner
#include <algorithm>                      // for move

namespace RawSpeed {

class CameraMetaData;

class Cr2Decoder final : public AbstractTiffDecoder
{
public:
  // please revert _this_ commit, once IWYU can handle inheriting constructors
  // using AbstractTiffDecoder::AbstractTiffDecoder;
  Cr2Decoder(TiffRootIFDOwner&& root, FileMap* file)
    : AbstractTiffDecoder(move(root), file) {}

  RawImage decodeRawInternal() override;
  void checkSupportInternal(CameraMetaData *meta) override;
  void decodeMetaDataInternal(CameraMetaData *meta) override;

protected:
  int sraw_coeffs[3];

  int getDecoderVersion() const override { return 8; }
  RawImage decodeOldFormat();
  RawImage decodeNewFormat();
  void sRawInterpolate();
  int getHue();

  void interpolate_422_v0(int w, int h, int start_h, int end_h);
  void interpolate_422_v1(int w, int h, int start_h, int end_h);
  void interpolate_420_v1(int w, int h, int start_h, int end_h);
  void interpolate_422_v2(int w, int h, int start_h, int end_h);
  void interpolate_420_v2(int w, int h, int start_h, int end_h);
};

} // namespace RawSpeed
