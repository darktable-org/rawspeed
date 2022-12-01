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
#include "adt/NORangesSet.h"             // for NORangesSet
#include "decoders/ArwDecoder.h"         // for ArwDecoder
#include "decoders/Cr2Decoder.h"         // for Cr2Decoder
#include "decoders/DcrDecoder.h"         // for DcrDecoder
#include "decoders/DcsDecoder.h"         // for DcsDecoder
#include "decoders/DngDecoder.h"         // for DngDecoder
#include "decoders/ErfDecoder.h"         // for ErfDecoder
#include "decoders/IiqDecoder.h"         // for IiqDecoder
#include "decoders/KdcDecoder.h"         // for KdcDecoder
#include "decoders/MefDecoder.h"         // for MefDecoder
#include "decoders/MosDecoder.h"         // for MosDecoder
#include "decoders/NefDecoder.h"         // for NefDecoder
#include "decoders/OrfDecoder.h"         // for OrfDecoder
#include "decoders/PefDecoder.h"         // for PefDecoder
#include "decoders/Rw2Decoder.h"         // for Rw2Decoder
#include "decoders/SrwDecoder.h"         // for SrwDecoder
#include "decoders/ThreefrDecoder.h"     // for ThreefrDecoder
#include "io/Buffer.h"                   // for Buffer, DataBuffer
#include "io/ByteStream.h"               // for ByteStream
#include "io/Endianness.h"               // for Endianness, Endianness::unk...
#include "parsers/TiffParserException.h" // for ThrowException, ThrowTPE
#include <cassert>                       // for assert
#include <cstdint>                       // for UINT32_MAX, uint16_t, uint32_t
#include <memory>                        // for make_unique, unique_ptr
#include <tuple>                         // for tie, tuple
#include <vector>                        // for vector
// IWYU pragma: no_include <ext/alloc_traits.h>

namespace rawspeed {
class RawDecoder;

TiffParser::TiffParser(const Buffer& file) : RawParser(file) {}

std::unique_ptr<RawDecoder> TiffParser::getDecoder(const CameraMetaData* meta) {
  return TiffParser::makeDecoder(TiffParser::parse(nullptr, mInput), mInput);
}

TiffRootIFDOwner TiffParser::parse(TiffIFD* parent, const Buffer& data) {
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
    root->add(std::make_unique<TiffIFD>(root.get(), &ifds, bs, IFDOffset));
  }

  return root;
}

std::unique_ptr<RawDecoder> TiffParser::makeDecoder(TiffRootIFDOwner root,
                                                    const Buffer& data) {
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
                                                    const Buffer& data) {
  return std::make_unique<Decoder>(std::move(root), data);
}

#define DECODER(name)                                                          \
  {                                                                            \
    std::make_pair(                                                            \
        static_cast<TiffParser::checker_t>(&name::isAppropriateDecoder),       \
        &constructor<name>)                                                    \
  }

const std::array<std::pair<TiffParser::checker_t, TiffParser::constructor_t>,
                 16>
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
        DECODER(ThreefrDecoder),

    }};

} // namespace rawspeed
