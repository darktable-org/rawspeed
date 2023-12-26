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

#include "common/RawImage.h"
#include "decoders/AbstractTiffDecoder.h"
#include "tiff/TiffIFD.h"
#include <cstdint>
#include <optional>
#include <vector>

namespace rawspeed {

class Buffer;
class CameraMetaData;
class iRectangle2D;
struct DngTilingDescription;

class DngDecoder final : public AbstractTiffDecoder {
public:
  static bool isAppropriateDecoder(const TiffRootIFD* rootIFD, Buffer file);
  DngDecoder(TiffRootIFDOwner&& rootIFD, Buffer file);

  RawImage decodeRawInternal() override;
  void decodeMetaDataInternal(const CameraMetaData* meta) override;
  void checkSupportInternal(const CameraMetaData* meta) override;

private:
  [[nodiscard]] int getDecoderVersion() const override { return 0; }
  bool mFixLjpeg;
  static void dropUnsuportedChunks(std::vector<const TiffIFD*>* data);
  std::optional<iRectangle2D> parseACTIVEAREA(const TiffIFD* raw) const;
  void parseCFA(const TiffIFD* raw) const;
  void parseColorMatrix() const;
  void parseWhiteBalance() const;
  DngTilingDescription getTilingDescription(const TiffIFD* raw) const;
  void decodeData(const TiffIFD* raw, uint32_t sample_format) const;
  void handleMetadata(const TiffIFD* raw);
  bool decodeMaskedAreas(const TiffIFD* raw) const;
  void decodeBlackLevelDelta(const TiffIFD* raw, TiffTag tag, int patSize,
                             int dimSize, auto z) const;
  bool decodeBlackLevels(const TiffIFD* raw) const;
  void setBlack(const TiffIFD* raw) const;

  int bps = -1;
  int compression = -1;
};

} // namespace rawspeed
