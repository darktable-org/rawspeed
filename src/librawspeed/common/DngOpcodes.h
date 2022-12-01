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

#include <cstdint> // for uint32_t
#include <map>     // for map
#include <memory>  // for unique_ptr
#include <utility> // for pair
#include <vector>  // for vector

namespace rawspeed {

class ByteStream;
class RawImage;
class iRectangle2D;

class DngOpcodes {
public:
  DngOpcodes(const RawImage& ri, ByteStream bs);
  ~DngOpcodes();
  void applyOpCodes(const RawImage& ri) const;

private:
  class DngOpcode;

  std::vector<std::unique_ptr<DngOpcode>> opcodes;

protected:
  class DeltaRowOrColBase;
  class DummyROIOpcode;
  class FixBadPixelsConstant;
  class FixBadPixelsList;
  class LookupOpcode;
  class PixelOpcode;
  class PolynomialMap;
  class ROIOpcode;
  class TableMap;
  class TrimBounds;
  template <typename S> class DeltaRowOrCol;
  template <typename S> class OffsetPerRowOrCol;
  template <typename S> class ScalePerRowOrCol;

  template <class Opcode>
  static std::unique_ptr<DngOpcode>
  constructor(const RawImage& ri, ByteStream& bs,
              iRectangle2D& integrated_subimg);

  using constructor_t = std::unique_ptr<DngOpcode> (*)(
      const RawImage& ri, ByteStream& bs, iRectangle2D& integrated_subimg);
  static const std::map<uint32_t, std::pair<const char*, constructor_t>> Map;
};

} // namespace rawspeed
