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

#include "common/Common.h"       // for uint32, uchar8
#include "common/RawImage.h"     // for RawImage
#include "decoders/RawDecoder.h" // for RawDecoder
#include "tiff/CiffIFD.h"        // for CiffIFD
#include <array>                 // for array
#include <memory>                // for unique_ptr

namespace RawSpeed {

class Buffer;

class CameraMetaData;

class HuffmanTable;

class CrwDecoder final : public RawDecoder {
public:
  CrwDecoder(std::unique_ptr<CiffIFD> rootIFD, Buffer* file);
  RawImage decodeRawInternal() override;
  void checkSupportInternal(const CameraMetaData* meta) override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;

protected:
  std::unique_ptr<CiffIFD> mRootIFD;
  int getDecoderVersion() const override { return 0; }
  void decodeRaw(bool lowbits, uint32 dec_table, uint32 width, uint32 height);
  static float canonEv(long in);
  static HuffmanTable makeDecoder(int n, const uchar8* source);
  static std::array<HuffmanTable, 2> initHuffTables(uint32 table);
};

} // namespace RawSpeed
