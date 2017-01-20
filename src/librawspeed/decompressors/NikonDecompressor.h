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

#ifndef NIKON_DECOMPRESSOR_H
#define NIKON_DECOMPRESSOR_H

#include "common/RawImage.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"

namespace RawSpeed {

void decompressNikon(RawImage& mRaw, ByteStream&& data, ByteStream meta, uint32 w, uint32 h, uint32 bitsPS, bool uncorrectedRawValues);
  
} // namespace RawSpeed

#endif
