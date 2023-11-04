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
#include "io/Buffer.h"
#include "parsers/RawParser.h"
#include "tiff/TiffIFD.h"
#include <array>
#include <memory>
#include <utility>

namespace rawspeed {

class Buffer;
class CameraMetaData;
class RawDecoder;

class TiffParser final : public RawParser {
public:
  explicit TiffParser(Buffer file);

  std::unique_ptr<RawDecoder>
  getDecoder(const CameraMetaData* meta = nullptr) override;

  // TiffRootIFDOwner contains pointers into 'data' but if is is non-owning, it
  // may be deleted immediately
  static TiffRootIFDOwner parse(TiffIFD* parent, Buffer data);

  // transfers ownership of TiffIFD into RawDecoder
  static std::unique_ptr<RawDecoder> makeDecoder(TiffRootIFDOwner root,
                                                 Buffer data);

  template <class Decoder>
  static std::unique_ptr<RawDecoder> constructor(TiffRootIFDOwner&& root,
                                                 Buffer data);
  using checker_t = bool (*)(const TiffRootIFD* root, Buffer data);
  using constructor_t = std::unique_ptr<RawDecoder> (*)(TiffRootIFDOwner&& root,
                                                        Buffer data);
  static const std::array<std::pair<checker_t, constructor_t>, 17> Map;
};

} // namespace rawspeed
