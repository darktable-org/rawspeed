/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev
    Copyright (C) 2021 Daniel Vogelbacher

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

#include "parsers/IsoMParser.h"          // For IsoMParser
#include "decoders/RawDecoder.h"         // for RawDecoder
#include "io/ByteStream.h"               // for ByteStream
#include "io/Endianness.h"               // for Endianness::big
#include "parsers/IsoMParserException.h" // for ThrowIPE

namespace rawspeed {

IsoMParser::IsoMParser(const Buffer* inputData) : RawParser(inputData) {}

void IsoMParser::parseData() {
  ByteStream bs(DataBuffer(*mInput, Endianness::unknown));

  // The 'ISO base media file format' is big-endian.
  bs.setByteOrder(Endianness::big);

  // *Everything* is the box.
  auto box = std::make_unique<IsoMRootBox>(&bs);
  // It should have consumed all of the buffer.
  assert(bs.getRemainSize() == 0);

  box->parse();

  rootBox = std::move(box);
}

std::unique_ptr<RawDecoder> IsoMParser::getDecoder(const CameraMetaData* meta) {
  if (!rootBox)
    parseData();


  ThrowIPE("No decoder found. Sorry.");
}

} // namespace rawspeed
