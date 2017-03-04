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

#include "common/Common.h"                // for ushort16
#include "common/RawImage.h"              // for RawImage
#include "decoders/AbstractTiffDecoder.h" // for AbstractTiffDecoder
#include "tiff/TiffIFD.h"                 // for TiffRootIFDOwner
#include <algorithm>                      // for move

namespace RawSpeed {

class CameraMetaData;

class Buffer;

class Cr2Decoder final : public AbstractTiffDecoder
{
public:
  // please revert _this_ commit, once IWYU can handle inheriting constructors
  // using AbstractTiffDecoder::AbstractTiffDecoder;
  Cr2Decoder(TiffRootIFDOwner&& root, Buffer* file)
      : AbstractTiffDecoder(move(root), file) {}

  RawImage decodeRawInternal() override;
  void checkSupportInternal(const CameraMetaData* meta) override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;

protected:
  int getDecoderVersion() const override { return 8; }
  RawImage decodeOldFormat();
  RawImage decodeNewFormat();
  void sRawInterpolate();
  int getHue();

  static inline void STORE_RGB(ushort16* X, int r, int g, int b, int offset);

  template <int version>
  static inline void YUV_TO_RGB(int Y, int Cb, int Cr, const int* sraw_coeffs,
                                ushort16* X, int offset);

  template <int version>
  static inline void interpolate_422(const int* sraw_coeffs, RawImage& mRaw,
                                     int hue, int hue_last, int w, int h);
  template <int version>
  static inline void interpolate_420(const int* sraw_coeffs, RawImage& mRaw,
                                     int hue, int w, int h);

  template <int version>
  static void interpolate_422(int hue, RawImage& mRaw, int* sraw_coeffs, int w,
                              int h);
  template <int version>
  static void interpolate_420(int hue, RawImage& mRaw, int* sraw_coeffs, int w,
                              int h);
};

} // namespace RawSpeed
