/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev
    Copyright (C) 2021 Daniel Vogelbacher

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
#include "tiff/IsoMBox.h"      // for IsoMRootBox
#include <memory>              // for unique_ptr

namespace rawspeed {

class Buffer;

class RawDecoder;

class CameraMetaData;

class IsoMParser final : public RawParser {
  std::unique_ptr<const IsoMRootBox> rootBox;

  void parseData();

public:
  explicit IsoMParser(const Buffer& input);

  std::unique_ptr<RawDecoder>
  getDecoder(const CameraMetaData* meta = nullptr) override;
};

} // namespace rawspeed
