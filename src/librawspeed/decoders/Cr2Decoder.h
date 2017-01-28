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

#include "decoders/RawDecoder.h"
#include "decompressors/LJpegPlain.h"
#include "tiff/TiffIFD.h"

namespace RawSpeed {

class Cr2Decoder final :
  public RawDecoder
{
public:
  Cr2Decoder(TiffIFD *rootIFD, FileMap* file);
  RawImage decodeRawInternal() override;
  void checkSupportInternal(CameraMetaData *meta) override;
  void decodeMetaDataInternal(CameraMetaData *meta) override;
  TiffIFD *getRootIFD() override { return mRootIFD; }
  ~Cr2Decoder() override;

protected:
  int sraw_coeffs[3];

  RawImage decodeOldFormat();
  RawImage decodeNewFormat();
  void sRawInterpolate();
  int getHue();

  using yuv2rgb = void(int Y, int Cb, int Cr, int& r, int& g, int& b,
                       const int* sraw_coeffs);
  static yuv2rgb YUV_TO_RGB_v0;
  static yuv2rgb YUV_TO_RGB_v1;
  static yuv2rgb YUV_TO_RGB_v2;

  void interpolate_420(int w, int h, int start_h , int end_h);
  void interpolate_422(int w, int h, int start_h , int end_h);
  void interpolate_422_old(int w, int h, int start_h , int end_h);
  void interpolate_420_new(int w, int h, int start_h , int end_h);
  void interpolate_422_new(int w, int h, int start_h , int end_h);
  TiffIFD *mRootIFD;
};

} // namespace RawSpeed
