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
#include "common/Common.h"
#include "decoders/CrwDecoder.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "parsers/CiffParserException.h"
#include "parsers/RawParser.h"
#include "tiff/CiffEntry.h"
#include "tiff/CiffIFD.h"
#include "tiff/CiffTag.h"
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace rawspeed {

CiffParser::CiffParser(Buffer inputData) : RawParser(inputData) {}

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

  for (const auto potentials(mRootIFD->getIFDsWithTag(CiffTag::MAKEMODEL));
       const auto& potential : potentials) {
    const auto* const mm = potential->getEntry(CiffTag::MAKEMODEL);
    const std::string make = trimSpaces(mm->getString());

    if (make == "Canon")
      return std::make_unique<CrwDecoder>(std::move(mRootIFD), mInput);
  }

  ThrowCPE("No decoder found. Sorry.");
}

} // namespace rawspeed
