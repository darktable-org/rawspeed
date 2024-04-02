/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2017 Axel Waggershauser
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

#include "parsers/TiffParser.h"
#include "adt/NORangesSet.h"
#include "decoders/ArwDecoder.h"
#include "decoders/Cr2Decoder.h"
#include "decoders/DcrDecoder.h"
#include "decoders/DcsDecoder.h"
#include "decoders/DngDecoder.h"
#include "decoders/ErfDecoder.h"
#include "decoders/IiqDecoder.h"
#include "decoders/KdcDecoder.h"
#include "decoders/MefDecoder.h"
#include "decoders/MosDecoder.h"
#include "decoders/NefDecoder.h"
#include "decoders/OrfDecoder.h"
#include "decoders/PefDecoder.h"
#include "decoders/Rw2Decoder.h"
#include "decoders/SrwDecoder.h"
#include "decoders/StiDecoder.h"
#include "decoders/ThreefrDecoder.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "parsers/RawParser.h"
#include "parsers/TiffParserException.h"
#include "tiff/TiffIFD.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

namespace rawspeed {
class RawDecoder;

TiffParser::TiffParser(Buffer file) : RawParser(file) {}

std::unique_ptr<RawDecoder> TiffParser::getDecoder(const CameraMetaData* meta) {
  return TiffParser::makeDecoder(TiffParser::parse(nullptr, mInput), mInput);
}

TiffRootIFDOwner TiffParser::parse(TiffIFD* parent, Buffer data) {
  ByteStream bs(DataBuffer(data, Endianness::unknown));
  bs.setByteOrder(getTiffByteOrder(bs, 0, "TIFF header"));
  bs.skipBytes(2);

  if (uint16_t magic = bs.getU16();
      magic != 42 && magic != 0x4f52 && magic != 0x5352 &&
      magic != 0x55) // ORF has 0x4f52/0x5352, RW2 0x55 - Brilliant!
    ThrowTPE("Not a TIFF file (magic 42)");

  auto root = std::make_unique<TiffRootIFD>(
      parent, nullptr, bs,
      UINT32_MAX); // tell TiffIFD constructor not to parse bs as IFD

  NORangesSet<Buffer> ifds;

  for (uint32_t IFDOffset = bs.getU32(); IFDOffset;
       IFDOffset = root->getSubIFDs().back()->getNextIFD()) {
    std::unique_ptr<TiffIFD> subIFD;
    try {
      subIFD = std::make_unique<TiffIFD>(root.get(), &ifds, bs, IFDOffset);
    } catch (const TiffParserException&) {
      // This IFD may fail to parse, in which case exit the loop,
      // because the offset to the next IFD is last 4 bytes of an IFD,
      // and we didn't get them because the IFD failed to parse.
      // BUT: don't discard the IFD's that did succeed to parse!
      break;
    }
    assert(subIFD.get());
    root->add(std::move(subIFD));
  }

  return root;
}

std::unique_ptr<RawDecoder> TiffParser::makeDecoder(TiffRootIFDOwner root,
                                                    Buffer data) {
  if (!root)
    ThrowTPE("TiffIFD is null.");

  for (const auto& decoder : Map) {
    checker_t dChecker = nullptr;
    constructor_t dConstructor = nullptr;

    std::tie(dChecker, dConstructor) = decoder;

    assert(dChecker);
    assert(dConstructor);

    if (!dChecker(root.get(), data))
      continue;

    return dConstructor(std::move(root), data);
  }

  ThrowTPE("No decoder found. Sorry.");
}

template <class Decoder>
std::unique_ptr<RawDecoder> TiffParser::constructor(TiffRootIFDOwner&& root,
                                                    Buffer data) {
  return std::make_unique<Decoder>(std::move(root), data);
}

#define DECODER(name)                                                          \
  {                                                                            \
    std::make_pair(                                                            \
        static_cast<TiffParser::checker_t>(&name::isAppropriateDecoder),       \
        &constructor<name>)                                                    \
  }

const std::array<std::pair<TiffParser::checker_t, TiffParser::constructor_t>,
                 17>
    TiffParser::Map = {{
        DECODER(DngDecoder),
        DECODER(MosDecoder),
        DECODER(IiqDecoder),
        DECODER(Cr2Decoder),
        DECODER(NefDecoder),
        DECODER(OrfDecoder),
        DECODER(ArwDecoder),
        DECODER(PefDecoder),
        DECODER(Rw2Decoder),
        DECODER(SrwDecoder),
        DECODER(MefDecoder),
        DECODER(DcrDecoder),
        DECODER(DcsDecoder),
        DECODER(KdcDecoder),
        DECODER(ErfDecoder),
        DECODER(StiDecoder),
        DECODER(ThreefrDecoder),

    }};

} // namespace rawspeed
