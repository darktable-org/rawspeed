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
#include "common/Common.h"               // for make_unique, trimSpaces
#include "decoders/ArwDecoder.h"         // for ArwDecoder
#include "decoders/Cr2Decoder.h"         // for Cr2Decoder
#include "decoders/DcrDecoder.h"         // for DcrDecoder
#include "decoders/DcsDecoder.h"         // for DcsDecoder
#include "decoders/DngDecoder.h"         // for DngDecoder
#include "decoders/ErfDecoder.h"         // for ErfDecoder
#include "decoders/KdcDecoder.h"         // for KdcDecoder
#include "decoders/MefDecoder.h"         // for MefDecoder
#include "decoders/MosDecoder.h"         // for MosDecoder
#include "decoders/NefDecoder.h"         // for NefDecoder
#include "decoders/OrfDecoder.h"         // for OrfDecoder
#include "decoders/PefDecoder.h"         // for PefDecoder
#include "decoders/RafDecoder.h"         // for RafDecoder
#include "decoders/RawDecoder.h"         // for RawDecoder
#include "decoders/Rw2Decoder.h"         // for Rw2Decoder
#include "decoders/SrwDecoder.h"         // for SrwDecoder
#include "decoders/ThreefrDecoder.h"     // for ThreefrDecoder
#include "io/ByteStream.h"               // for ByteStream
#include "parsers/TiffParserException.h" // for TiffParserException
#include <cassert>                       // for assert
#include <cstdint>                       // for UINT32_MAX
#include <memory>                        // for unique_ptr
#include <string>                        // for operator==, basic_string
#include <tuple>                         // for tie, tuple
#include <vector>                        // for vector
// IWYU pragma: no_include <ext/alloc_traits.h>

using std::string;

namespace rawspeed {

TiffParser::TiffParser(const Buffer* file) : RawParser(file) {}

std::unique_ptr<RawDecoder> TiffParser::getDecoder(const CameraMetaData* meta) {
  return TiffParser::makeDecoder(TiffParser::parse(*mInput), *mInput);
}

TiffRootIFDOwner TiffParser::parse(const Buffer& data) {
  ByteStream bs(data, 0);
  bs.setInNativeByteOrder(isTiffInNativeByteOrder(bs, 0, "TIFF header"));
  bs.skipBytes(2);

  ushort16 magic = bs.getU16();
  if (magic != 42 && magic != 0x4f52 && magic != 0x5352 && magic != 0x55) // ORF has 0x4f52/0x5352, RW2 0x55 - Brillant!
    ThrowTPE("Not a TIFF file (magic 42)");

  TiffRootIFDOwner root = make_unique<TiffRootIFD>(
      nullptr, bs,
      UINT32_MAX); // tell TiffIFD constructur not to parse bs as IFD
  for( uint32 nextIFD = bs.getU32(); nextIFD; nextIFD = root->getSubIFDs().back()->getNextIFD() ) {
    root->add(make_unique<TiffIFD>(root.get(), bs, nextIFD));
  }

  return root;
}

std::unique_ptr<RawDecoder> TiffParser::makeDecoder(TiffRootIFDOwner root,
                                                    const Buffer& data) {
  const Buffer* mInput = &data;
  if (!root)
    ThrowTPE("TiffIFD is null.");

  for (const auto& decoder : Map) {
    checker_t dChecker = nullptr;
    constructor_t dConstructor = nullptr;

    std::tie(dChecker, dConstructor) = decoder;

    assert(dChecker);
    assert(dConstructor);

    if (!dChecker(root.get(), mInput))
      continue;

    return dConstructor(move(root), mInput);
  }

  ThrowTPE("No decoder found. Sorry.");
  return nullptr;
}

template <class Decoder>
std::unique_ptr<RawDecoder> TiffParser::constructor(TiffRootIFDOwner&& root,
                                                    const Buffer* data) {
  return make_unique<Decoder>(std::move(root), data);
}

#define DECODER(name)                                                          \
  { std::make_pair(&name::isAppropriateDecoder, &constructor<name>) }

const std::array<std::pair<TiffParser::checker_t, TiffParser::constructor_t>,
                 16>
    TiffParser::Map = {{
        DECODER(DngDecoder), DECODER(MosDecoder), DECODER(Cr2Decoder),
        DECODER(RafDecoder), DECODER(NefDecoder), DECODER(OrfDecoder),
        DECODER(ArwDecoder), DECODER(PefDecoder), DECODER(Rw2Decoder),
        DECODER(SrwDecoder), DECODER(MefDecoder), DECODER(DcrDecoder),
        DECODER(DcsDecoder), DECODER(KdcDecoder), DECODER(ErfDecoder),
        DECODER(ThreefrDecoder),

    }};

} // namespace rawspeed
