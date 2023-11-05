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

// IWYU pragma: begin_exports

#include "codes/AbstractPrefixCode.h"
#include "codes/PrefixCodeLUTDecoder.h"
#include "codes/PrefixCodeLookupDecoder.h"
// #include "codes/PrefixCodeLookupDecoder.h"
// #include "codes/PrefixCodeTreeDecoder.h"
// #include "codes/PrefixCodeVectorDecoder.h"

// IWYU pragma: end_exports

namespace rawspeed {

template <typename CodeTag = BaselineCodeTag>
using PrefixCodeDecoder =
    PrefixCodeLUTDecoder<CodeTag, PrefixCodeLookupDecoder<CodeTag>>;

// template <typename CodeTag>
// using PrefixCodeDecoder = PrefixCodeLookupDecoder<CodeTag>;

// template <typename CodeTag>
// using PrefixCodeDecoder = PrefixCodeTreeDecoder<CodeTag>;

// template <typename CodeTag>
// using PrefixCodeDecoder = PrefixCodeVectorDecoder<CodeTag>;

} // namespace rawspeed
