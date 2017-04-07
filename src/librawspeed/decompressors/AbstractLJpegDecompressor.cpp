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

#include "decompressors/AbstractLJpegDecompressor.h"
#include "common/Common.h"                // for uint32, make_unique, uchar8
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/ByteStream.h"                // for ByteStream
#include <array>                          // for array
#include <memory>                         // for unique_ptr, allocator
#include <utility>                        // for move
#include <vector>                         // for vector

namespace rawspeed {

void AbstractLJpegDecompressor::decode() {
  if (getNextMarker(false) != M_SOI)
    ThrowRDE("Image did not start with SOI. Probably not an LJPEG");

  JpegMarker m;
  do {
    m = getNextMarker(true);

    switch (m) {
    case M_DHT:  parseDHT(); break;
    case M_SOF3: parseSOF(&frame); break;
    case M_SOS:  parseSOS(); break;
    case M_DQT:
      ThrowRDE("Not a valid RAW file.");
      break;
    default:  // Just let it skip to next marker
      break;
    }
  } while (m != M_EOI);
}

void AbstractLJpegDecompressor::parseSOF(SOFInfo* sof) {
  uint32 headerLength = input.getU16();
  sof->prec = input.getByte();
  sof->h = input.getU16();
  sof->w = input.getU16();
  sof->cps = input.getByte();

  if (sof->h == 0 || sof->w == 0)
    ThrowRDE("Frame width or height set to zero");

  if (sof->prec > 16)
    ThrowRDE("More than 16 bits per channel is not supported.");

  if (sof->cps > 4 || sof->cps < 1)
    ThrowRDE("Only from 1 to 4 components are supported.");

  if (headerLength != 8 + sof->cps*3)
    ThrowRDE("Header size mismatch.");

  for (uint32 i = 0; i < sof->cps; i++) {
    sof->compInfo[i].componentId = input.getByte();
    uint32 subs = input.getByte();
    frame.compInfo[i].superV = subs & 0xf;
    frame.compInfo[i].superH = subs >> 4;
    uint32 Tq = input.getByte();
    if (Tq != 0)
      ThrowRDE("Quantized components not supported.");
  }
  sof->initialized = true;

  mRaw->metadata.subsampling.x = sof->compInfo[0].superH;
  mRaw->metadata.subsampling.y = sof->compInfo[0].superV;
}

void AbstractLJpegDecompressor::parseSOS() {
  if (!frame.initialized)
    ThrowRDE("Frame not yet initialized (SOF Marker not parsed)");

  uint32 headerLength = input.getU16();
  if (headerLength != 3 + frame.cps * 2 + 3)
    ThrowRDE("Invalid SOS header length.");

  uint32 soscps = input.getByte();
  if (frame.cps != soscps)
    ThrowRDE("Component number mismatch.");

  for (uint32 i = 0; i < frame.cps; i++) {
    uint32 cs = input.getByte();
    uint32 td = input.getByte() >> 4;

    if (td > 3 || !huff[td])
      ThrowRDE("Invalid Huffman table selection.");

    int ciIndex = -1;
    for (uint32 j = 0; j < frame.cps; ++j) {
      if (frame.compInfo[j].componentId == cs)
        ciIndex = j;
    }

    if (ciIndex == -1)
      ThrowRDE("Invalid Component Selector");

    frame.compInfo[ciIndex].dcTblNo = td;
  }

  // Get predictor, see table H.1 from the JPEG spec
  predictorMode = input.getByte();
  // The spec says predictoreMode is in [0..7], but Hasselblad uses '8'.
  if (predictorMode > 8)
    ThrowRDE("Invalid predictor mode.");

  input.skipBytes(1);         // Se + Ah Not used in LJPEG
  Pt = input.getByte() & 0xf; // Point Transform

  decodeScan();
}

void AbstractLJpegDecompressor::parseDHT() {
  uint32 headerLength = input.getU16() - 2; // Subtract myself

  while (headerLength)  {
    uint32 b = input.getByte();

    uint32 htClass = b >> 4;
    if (htClass != 0)
      ThrowRDE("Unsupported Table class.");

    uint32 htIndex = b & 0xf;
    if (htIndex >= huff.size())
      ThrowRDE("Invalid huffman table destination id.");

    if (huff[htIndex] != nullptr)
      ThrowRDE("Duplicate table definition");

    // copy 16 bytes from input stream to number of codes per length table
    uint32 nCodes = ht_.setNCodesPerLength(input.getBuffer(16));
    // spec says 16 different codes is max but Hasselblad violates that -> 17
    if (nCodes > 17 || headerLength < 1 + 16 + nCodes)
      ThrowRDE("Invalid DHT table.");

    // copy nCodes bytes from input stream to code values table
    ht_.setCodeValues(input.getBuffer(nCodes));

    // see if we already have a HuffmanTable with the same codes
    for (const auto& i : huffmanTableStore)
      if (*i == ht_)
        huff[htIndex] = i.get();

    if (!huff[htIndex]) {
      // setup new ht_ and put it into the store
      auto dHT = make_unique<HuffmanTable>(ht_);
      dHT->setup(fullDecodeHT, fixDng16Bug);
      huff[htIndex] = dHT.get();
      huffmanTableStore.emplace_back(std::move(dHT));
    }
    headerLength -= 1 + 16 + nCodes;
  }
}

JpegMarker AbstractLJpegDecompressor::getNextMarker(bool allowskip) {
  uchar8 c0, c1 = input.getByte();
  do {
    c0 = c1;
    c1 = input.getByte();
  } while (allowskip && !(c0 == 0xFF && c1 != 0 && c1 != 0xFF));

  if (!(c0 == 0xFF && c1 != 0 && c1 != 0xFF))
    ThrowRDE("(Noskip) Expected marker not found. Propably corrupt file.");

  return static_cast<JpegMarker>(c1);
}

} // namespace rawspeed
