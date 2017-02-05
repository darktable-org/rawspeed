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

#include "decoders/ThreefrDecoder.h"
#include "common/Common.h"                        // for uint32
#include "common/Point.h"                         // for iPoint2D
#include "decoders/RawDecoderException.h"         // for ThrowRDE
#include "decompressors/HasselbladDecompressor.h" // for HasselbladDecompre...
#include "io/IOException.h"                       // for IOException
#include "metadata/ColorFilterArray.h"            // for CFAColor::CFA_GREEN
#include "tiff/TiffEntry.h"                       // for TiffEntry
#include "tiff/TiffIFD.h"                         // for TiffRootIFD, TiffIFD
#include "tiff/TiffTag.h"                         // for TiffTag::ASSHOTNEU...
#include <map>                                    // for _Rb_tree_iterator
#include <memory>                                 // for unique_ptr
#include <string>                                 // for string, stoi
#include <utility>                                // for pair
#include <vector>                                 // for vector

using namespace std;

namespace RawSpeed {

class CameraMetaData;

RawImage ThreefrDecoder::decodeRawInternal() {
  auto raw = mRootIFD->getIFDWithTag(STRIPOFFSETS, 1);
  uint32 width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32 height = raw->getEntry(IMAGELENGTH)->getU32();
  uint32 off = raw->getEntry(STRIPOFFSETS)->getU32();

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  HasselbladDecompressor l(*mFile, off, mRaw);
  int pixelBaseOffset = 0;
  auto pixelOffsetHint = hints.find("pixelBaseOffset");
  if (pixelOffsetHint != hints.end())
    pixelBaseOffset = stoi(pixelOffsetHint->second);

  try {
    l.decode(pixelBaseOffset);
  } catch (IOException &e) {
    mRaw->setError(e.what());
    // Let's ignore it, it may have delivered somewhat useful data.
  }

  return mRaw;
}

void ThreefrDecoder::decodeMetaDataInternal(CameraMetaData *meta) {
  mRaw->cfa.setCFA(iPoint2D(2,2), CFA_RED, CFA_GREEN, CFA_GREEN, CFA_BLUE);

  setMetaData(meta, "", 0);

  // Fetch the white balance
  if (mRootIFD->hasEntryRecursive(ASSHOTNEUTRAL)) {
    TiffEntry *wb = mRootIFD->getEntryRecursive(ASSHOTNEUTRAL);
    if (wb->count == 3) {
      for (uint32 i=0; i<3; i++)
        mRaw->metadata.wbCoeffs[i] = 1.0f/wb->getFloat(i);
    }
  }
}

} // namespace RawSpeed
