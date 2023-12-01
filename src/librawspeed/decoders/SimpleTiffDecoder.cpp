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

#include "decoders/SimpleTiffDecoder.h"
#include "adt/Point.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "io/Buffer.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"

namespace rawspeed {

void SimpleTiffDecoder::anchor() const {
  // Empty out-of-line definition for the purpose of anchoring
  // the class's vtable to this Translational Unit.
}

void SimpleTiffDecoder::prepareForRawDecoding() {
  raw = getIFDWithLargestImage();
  width = raw->getEntry(TiffTag::IMAGEWIDTH)->getU32();
  height = raw->getEntry(TiffTag::IMAGELENGTH)->getU32();
  off = raw->getEntry(TiffTag::STRIPOFFSETS)->getU32();
  c2 = raw->getEntry(TiffTag::STRIPBYTECOUNTS)->getU32();

  if (!mFile.isValid(off, c2))
    ThrowRDE("Image is truncated.");

  if (c2 == 0)
    ThrowRDE("No image data found.");

  if (0 == width || 0 == height)
    ThrowRDE("Image has zero size.");

  checkImageDimensions();

  mRaw->dim = iPoint2D(width, height);
}

} // namespace rawspeed
