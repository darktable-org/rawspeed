/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Roman Lebedev

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

#include "parsers/RawParser.h" // for RawParser
#include "tiff/TiffIFD.h"      // for TiffRootIFDOwner
#include <memory>              // for unique_ptr

namespace rawspeed {

class Buffer;

class CameraMetaData;

class RawDecoder;

class FiffParser final : public RawParser {
  TiffRootIFDOwner rootIFD;

public:
  explicit FiffParser(const Buffer& input);

  void parseData();
  std::unique_ptr<RawDecoder>
  getDecoder(const CameraMetaData* meta = nullptr) override;
};

} // namespace rawspeed
