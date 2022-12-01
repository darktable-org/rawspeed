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

#include "parsers/RawParser.h"
#include "decoders/MrwDecoder.h"          // for MrwDecoder
#include "decoders/NakedDecoder.h"        // for NakedDecoder
#include "decoders/RafDecoder.h"          // for RafDecoder
#include "decoders/RawDecoder.h"          // for RawDecoder
#include "decoders/RawDecoderException.h" // for RawDecoderException, ThrowRDE
#include "io/Buffer.h"                    // for Buffer
#include "metadata/CameraMetaData.h"      // for CameraMetaData
#include "parsers/CiffParser.h"           // for CiffParser
#include "parsers/CiffParserException.h"  // for CiffParserException
#include "parsers/FiffParser.h"           // for FiffParser
#include "parsers/FiffParserException.h"  // for FiffParserException
#include "parsers/TiffParser.h"           // for TiffParser
#include "parsers/TiffParserException.h"  // for ThrowException, TiffParser...

namespace rawspeed {

class Camera;

std::unique_ptr<RawDecoder> RawParser::getDecoder(const CameraMetaData* meta) {
  // We need some data.
  // For now it is 104 bytes for RAF/FUJIFIM images.
  // FIXME: each decoder/parser should check it on their own.
  if (mInput.getSize() <= 104)
    ThrowRDE("File too small");

  // MRW images are easy to check for, let's try that first
  if (MrwDecoder::isMRW(mInput)) {
    try {
      return std::make_unique<MrwDecoder>(mInput);
    } catch (const RawDecoderException&) {
    }
  }

  // FUJI has pointers to IFD's at fixed byte offsets
  // So if camera is FUJI, we cannot use ordinary TIFF parser
  if (RafDecoder::isRAF(mInput)) {
    try {
      FiffParser p(mInput);
      return p.getDecoder(meta);
    } catch (const FiffParserException&) {
    }
  }

  // Ordinary TIFF images
  try {
    TiffParser p(mInput);
    return p.getDecoder(meta);
  } catch (const TiffParserException&) {
  }

  // CIFF images
  try {
    CiffParser p(mInput);
    return p.getDecoder(meta);
  } catch (const CiffParserException&) {
  }

  // Detect camera on filesize (CHDK).
  if (meta != nullptr && meta->hasChdkCamera(mInput.getSize())) {
    const Camera* c = meta->getChdkCamera(mInput.getSize());

    try {
      return std::make_unique<NakedDecoder>(mInput, c);
    } catch (const RawDecoderException&) {
    }
  }

  // File could not be decoded, so no further options for now.
  ThrowRDE("No decoder found. Sorry.");
}

} // namespace rawspeed
