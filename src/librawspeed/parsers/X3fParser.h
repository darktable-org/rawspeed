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

#include "io/ByteStream.h"     // for ByteStream
#include "parsers/RawParser.h" // for RawParser
#include <memory>              // for unique_ptr
#include <string>              // for string

namespace rawspeed {

class Buffer;

class RawDecoder;

class CameraMetaData;

class X3fDecoder;

class X3fParser final : public RawParser {
  ByteStream bytes;

public:
  explicit X3fParser(Buffer* file);

  std::unique_ptr<RawDecoder>
  getDecoder(const CameraMetaData* meta = nullptr) override;

protected:
  void readDirectory(X3fDecoder* decoder);
  std::string getId();
};

} // namespace rawspeed
