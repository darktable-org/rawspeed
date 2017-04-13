/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2013 Klaus Post

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

#include "common/Common.h"       // for uint32, int32, uchar8, ushort16
#include "common/Point.h"        // for iPoint2D
#include "common/RawImage.h"     // for RawImage
#include "decoders/RawDecoder.h" // for RawDecoder, RawDecoderThread (ptr o...
#include "io/BitPumpMSB.h"       // for BitPumpMSB
#include "parsers/X3fParser.h"   // for X3fPropertyCollection, X3fDirectory
#include <array>                 // for array
#include <map>                   // for map, _Rb_tree_iterator
#include <string>                // for string
#include <vector>                // for vector

namespace rawspeed {

class ByteStream;
class CameraMetaData;
class Buffer;

class X3fDecoder final : public RawDecoder {
  ByteStream* bytes = nullptr;
  X3fImage* curr_image = nullptr;
  int pred[3];
  uint32 plane_sizes[3];
  uint32 plane_offset[3];
  iPoint2D planeDim[3];
  uchar8 code_table[256];
  int32 big_table[1 << 14];
  uint32* line_offsets = nullptr;
  ushort16* huge_table = nullptr;
  std::array<short, 1024> curve;
  uint32 max_len = 0;
  std::string camera_make;
  std::string camera_model;

public:
  explicit X3fDecoder(Buffer* file);
  ~X3fDecoder() override;

  RawImage decodeRawInternal() override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;
  void checkSupportInternal(const CameraMetaData* meta) override;

  Buffer* getCompressedData() override;
  std::vector<X3fDirectory> mDirectory;
  std::vector<X3fImage> mImages;
  X3fPropertyCollection mProperties;

protected:
  int getDecoderVersion() const override { return 1; }

  void decodeThreaded(RawDecoderThread *t) override;

  void readDirectory();
  std::string getId();

  bool hasProp(const char* key) {
    return mProperties.props.find(key) != mProperties.props.end();
  }
  std::string getProp(const char* key);

  void decompressSigma( X3fImage &image );
  void createSigmaTable(ByteStream *bytes, int codes);
  int SigmaDecode(BitPumpMSB *bits);
  std::string getIdAsString(ByteStream *bytes);
  void SigmaSkipOne(BitPumpMSB *bits);
  bool readName();
};

} // namespace rawspeed
