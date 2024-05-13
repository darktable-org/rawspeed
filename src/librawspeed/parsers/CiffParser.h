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

#include "parsers/RawParser.h"
#include "tiff/CiffIFD.h"
#include <memory>

namespace rawspeed {

class Buffer;
class CameraMetaData;
class RawDecoder;

class CiffParser final : public RawParser {
  std::unique_ptr<const CiffIFD> mRootIFD;

public:
  explicit CiffParser(Buffer input);

  void parseData();

  std::unique_ptr<RawDecoder>
  getDecoder(const CameraMetaData* meta = nullptr) override;
};

} // namespace rawspeed
