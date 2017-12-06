/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser

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

#include "common/Common.h" // for uint32
#include <map>             // for map
#include <memory>          // for unique_ptr
#include <utility>         // for pair
#include <vector>          // for vector

namespace rawspeed {

class RawImage;

class TiffEntry;

class ByteStream;

class DngOpcodes
{
public:
  DngOpcodes(const RawImage& ri, TiffEntry* entry);
  ~DngOpcodes();
  void applyOpCodes(const RawImage& ri);

private:
  class DngOpcode;
  std::vector<std::unique_ptr<DngOpcode>> opcodes;

protected:
  class FixBadPixelsConstant;
  class FixBadPixelsList;
  class ROIOpcode;
  class DummyROIOpcode;
  class TrimBounds;
  class PixelOpcode;
  class LookupOpcode;
  class TableMap;
  class PolynomialMap;
  class DeltaRowOrColBase;
  template <typename S> class DeltaRowOrCol;
  template <typename S> class OffsetPerRowOrCol;
  template <typename S> class ScalePerRowOrCol;

  template <class Opcode>
  static std::unique_ptr<DngOpcode> constructor(const RawImage& ri,
                                                ByteStream* bs);

  using constructor_t = std::unique_ptr<DngOpcode> (*)(const RawImage& ri,
                                                       ByteStream* bs);
  static const std::map<uint32, std::pair<const char*, constructor_t>> Map;
};

} // namespace rawspeed
