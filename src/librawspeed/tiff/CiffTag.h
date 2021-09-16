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

#include <initializer_list> // for initializer_list

namespace rawspeed {

enum class CiffTag {
  NULL_TAG = 0x0000,
  MAKEMODEL = 0x080a,
  SHOTINFO = 0x102a,
  WHITEBALANCE = 0x10a9,
  SENSORINFO = 0x1031,
  IMAGEINFO = 0x1810,
  DECODERTABLE = 0x1835,
  RAWDATA = 0x2005,
  SUBIFD = 0x300a,
  EXIF = 0x300b,
};

static constexpr std::initializer_list<CiffTag> CiffTagsWeCareAbout = {
    CiffTag::DECODERTABLE,        CiffTag::MAKEMODEL, CiffTag::RAWDATA,
    CiffTag::SENSORINFO,          CiffTag::SHOTINFO,  CiffTag::WHITEBALANCE,
    static_cast<CiffTag>(0x0032), // ???
    static_cast<CiffTag>(0x102c), // ???
};

} // namespace rawspeed
