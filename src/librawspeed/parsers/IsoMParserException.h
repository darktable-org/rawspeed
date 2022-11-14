/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

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

#include "common/RawspeedException.h"   // for ThrowExceptionHelper
#include "parsers/RawParserException.h" // for ThrowRPE, RawParserException
#include <string>

namespace rawspeed {

class IsoMParserException final : public RawParserException {
public:
  explicit IsoMParserException(const std::string& msg)
      : RawParserException(msg.c_str()) {}
  explicit IsoMParserException(const char* msg) : RawParserException(msg) {}
};

#define ThrowIPE(...)                                                          \
  ThrowExceptionHelper(rawspeed::IsoMParserException, __VA_ARGS__)

} // namespace rawspeed
