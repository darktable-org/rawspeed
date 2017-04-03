/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
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

#include "parsers/TiffParser.h"
#include "common/Common.h"               // for make_unique, trimSpaces
#include "common/RawspeedException.h"    // for RawspeedException
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
#include "decoders/Rw2Decoder.h"         // for Rw2Decoder
#include "decoders/SrwDecoder.h"         // for SrwDecoder
#include "decoders/ThreefrDecoder.h"     // for ThreefrDecoder
#include "io/ByteStream.h"               // for ByteStream
#include "parsers/TiffParserException.h" // for TiffParserException
#include "tiff/TiffEntry.h"              // for TiffEntry
#include "tiff/TiffTag.h"                // for TiffTag::DNGVERSION, TiffTa...
#include <algorithm>                     // for move
#include <cstdint>                       // for UINT32_MAX
#include <memory>                        // for unique_ptr
#include <string>                        // for operator==, basic_string
#include <vector>                        // for vector
// IWYU pragma: no_include <ext/alloc_traits.h>

using std::string;

namespace RawSpeed {

class RawDecoder;

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

RawDecoder* TiffParser::makeDecoder(TiffRootIFDOwner root, Buffer& data) {
  Buffer* mInput = &data;
  if (!root)
    ThrowTPE("TiffIFD is null.");

  if (root->hasEntryRecursive(DNGVERSION)) {  // We have a dng image entry
    try {
      return new DngDecoder(move(root), mInput);
    } catch (RawspeedException& e) {
      //TODO: remove this exception type conversion
      ThrowTPE("%s", e.what());
    }
  }

  try {
    auto id = root->getID();
    string make = id.make;
    string model = id.model;

    if (make == "Canon") {
      return new Cr2Decoder(move(root), mInput);
    }
    if (make == "FUJIFILM") {
      return new RafDecoder(move(root), mInput);
    }
    if (make == "NIKON CORPORATION" || make == "NIKON") {
      return new NefDecoder(move(root), mInput);
    }
    if (make == "OLYMPUS IMAGING CORP." || make == "OLYMPUS CORPORATION" ||
        make == "OLYMPUS OPTICAL CO.,LTD") {
      return new OrfDecoder(move(root), mInput);
    }
    if (make == "SONY") {
      return new ArwDecoder(move(root), mInput);
    }
    if (make == "PENTAX Corporation" || make == "RICOH IMAGING COMPANY, LTD." ||
        make == "PENTAX") {
      return new PefDecoder(move(root), mInput);
    }
    if (make == "Panasonic" || make == "LEICA") {
      return new Rw2Decoder(move(root), mInput);
    }
    if (make == "SAMSUNG") {
      return new SrwDecoder(move(root), mInput);
    }
    if (make == "Mamiya-OP Co.,Ltd.") {
      return new MefDecoder(move(root), mInput);
    }
    if (make == "Kodak") {
      if (model == "DCS560C")
        return new Cr2Decoder(move(root), mInput);

      return new DcrDecoder(move(root), mInput);
    }
    if (make == "KODAK") {
      return new DcsDecoder(move(root), mInput);
    }
    if (make == "EASTMAN KODAK COMPANY") {
      return new KdcDecoder(move(root), mInput);
    }
    if (make == "SEIKO EPSON CORP.") {
      return new ErfDecoder(move(root), mInput);
    }
    if (make == "Hasselblad") {
      return new ThreefrDecoder(move(root), mInput);
    }
    if (make == "Leaf" || make == "Phase One A/S") {
      return new MosDecoder(move(root), mInput);
    }
  } catch (const TiffParserException&) {
    // Last ditch effort to identify Leaf cameras that don't have a Tiff Make set
    TiffEntry* softwareIFD = root->getEntryRecursive(SOFTWARE);
    if (softwareIFD) {
      string software = trimSpaces(softwareIFD->getString());
      if (software == "Camera Library") {
        return new MosDecoder(move(root), mInput);
      }
    }
  }

  ThrowTPE("No decoder found. Sorry.");
  return nullptr;
}

} // namespace RawSpeed
