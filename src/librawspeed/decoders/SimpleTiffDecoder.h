/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
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

#include "decoders/AbstractTiffDecoder.h"
#include "io/Buffer.h"
#include "tiff/TiffIFD.h"
#include <cstdint>
#include <utility>

namespace rawspeed {

class Buffer;

class SimpleTiffDecoder : public AbstractTiffDecoder {
  void anchor() const final;

  virtual void checkImageDimensions() = 0;

public:
  SimpleTiffDecoder(TiffRootIFDOwner&& root, Buffer file)
      : AbstractTiffDecoder(std::move(root), file) {}

  void prepareForRawDecoding();

protected:
  const TiffIFD* raw;
  uint32_t width;
  uint32_t height;
  uint32_t off;
  uint32_t c2;
};

} // namespace rawspeed
