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

#ifndef TIFF_PARSER_H
#define TIFF_PARSER_H

#include "io/Buffer.h"
#include "tiff/TiffIFD.h"
#include "decoders/RawDecoder.h"

namespace RawSpeed {

// TiffRootIFDOwner contains pointers into 'data' but if is is non-owning, it may be deleted immediately
TiffRootIFDOwner parseTiff(const Buffer& data);

// transfers ownership of TiffIFD into RawDecoder
RawDecoder* makeDecoder(TiffRootIFDOwner root, Buffer &data);

} // namespace RawSpeed

#endif
