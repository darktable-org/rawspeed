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

#include "parsers/CiffParser.h"
#include "common/Common.h"               // for trimSpaces
#include "decoders/CrwDecoder.h"         // for CrwDecoder
#include "io/Buffer.h"                   // for Buffer (ptr only), DataBuffer
#include "io/ByteStream.h"               // for ByteStream
#include "io/Endianness.h"               // for Endianness, Endianness::little
#include "parsers/CiffParserException.h" // for ThrowException, ThrowCPE
#include "tiff/CiffEntry.h"              // for CiffEntry
#include "tiff/CiffIFD.h"                // for CiffIFD
#include "tiff/CiffTag.h"                // for CiffTag, CiffTag::MAKEMODEL
#include <cstdint>                       // for uint16_t, uint32_t
#include <string>                        // for operator==, string
#include <utility>                       // for move
#include <vector>                        // for vector

namespace rawspeed {

CiffParser::CiffParser(const Buffer& inputData) : RawParser(inputData) {}

void CiffParser::parseData() {
  ByteStream bs(DataBuffer(mInput, Endianness::little));

  if (const uint16_t byteOrder = bs.getU16();
      byteOrder != 0x4949) // "II" / little-endian
    ThrowCPE("Not a CIFF file (endianness)");

  // Offset to the beginning of the CIFF
  const uint32_t headerLength = bs.getU32();

  // 8 bytes of Signature
  if (!CrwDecoder::isCRW(mInput))
    ThrowCPE("Not a CIFF file (ID)");

  // *Everything* after the header is the root CIFF Directory
  ByteStream CIFFRootDirectory(bs.getSubStream(headerLength));
  mRootIFD = std::make_unique<CiffIFD>(nullptr, CIFFRootDirectory);
}

std::unique_ptr<RawDecoder> CiffParser::getDecoder(const CameraMetaData* meta) {
  if (!mRootIFD)
    parseData();

  const auto potentials(mRootIFD->getIFDsWithTag(CiffTag::MAKEMODEL));

  for (const auto& potential : potentials) {
    const auto* const mm = potential->getEntry(CiffTag::MAKEMODEL);
    const std::string make = trimSpaces(mm->getString());

    if (make == "Canon")
      return std::make_unique<CrwDecoder>(std::move(mRootIFD), mInput);
  }

  ThrowCPE("No decoder found. Sorry.");
}

} // namespace rawspeed
