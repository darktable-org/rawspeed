/*
    RawSpeed - RAW file decoder.

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

#include "decoders/AbstractTiffDecoder.h"
#include "decoders/RawDecoderException.h"
#include "tiff/TiffEntry.h"
#include "tiff/TiffIFD.h"
#include "tiff/TiffTag.h"
#include <cstdint>
#include <vector>

namespace rawspeed {

void AbstractTiffDecoder::anchor() const {
  // Empty out-of-line definition for the purpose of anchoring
  // the class's vtable to this Translational Unit.
}

const TiffIFD*
AbstractTiffDecoder::getIFDWithLargestImage(TiffTag filter) const {
  std::vector<const TiffIFD*> ifds = mRootIFD->getIFDsWithTag(filter);

  if (ifds.empty())
    ThrowRDE("No suitable IFD with tag 0x%04x found.",
             static_cast<unsigned>(filter));

  const auto* res = ifds[0];
  uint32_t width = res->getEntry(TiffTag::IMAGEWIDTH)->getU32();
  for (const auto* ifd : ifds) {
    const TiffEntry* widthE = ifd->getEntry(TiffTag::IMAGEWIDTH);
    // guard against random maker note entries with the same tag
    if (widthE->count == 1 && widthE->getU32() > width) {
      res = ifd;
      width = widthE->getU32();
    }
  }

  return res;
}

} // namespace rawspeed
