/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
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

  struct FoundMarkers {
    bool DHT = false;
    bool SOF = false;
    bool SOS = false;
  } FoundMarkers;

  JpegMarker m;
  do {
    m = getNextMarker(true);

    if (m == M_EOI)
      break;

    ByteStream data(input.getStream(input.peekU16()));
    data.skipBytes(2); // headerLength

    switch (m) {
    case M_DHT:
      if (FoundMarkers.SOS)
        ThrowRDE("Found second DHT marker after SOS");
      // there can be more than one DHT markers.
      // FIXME: do we really want to reparse and use the last one?
      parseDHT(data);
      FoundMarkers.DHT = true;
      break;
    case M_SOF3:
      if (FoundMarkers.SOS)
        ThrowRDE("Found second SOF marker after SOS");
      if (FoundMarkers.SOF)
        ThrowRDE("Found second SOF marker");
      // SOF is not required to be after DHT
      parseSOF(data, &frame);
      FoundMarkers.SOF = true;
      break;
    case M_SOS:
      if (FoundMarkers.SOS)
        ThrowRDE("Found second SOS marker");
      if (!FoundMarkers.DHT)
        ThrowRDE("Did not find DHT marker before SOS.");
      if (!FoundMarkers.SOF)
        ThrowRDE("Did not find SOF marker before SOS.");
      parseSOS(data);
      FoundMarkers.SOS = true;
      break;
    case M_DQT:
      ThrowRDE("Not a valid RAW file.");
    default: // Just let it skip to next marker
      break;
    }
  } while (m != M_EOI);

  if (!FoundMarkers.SOS)
    ThrowRDE("Did not find SOS marker.");
}

void AbstractLJpegDecompressor::parseSOF(ByteStream sofInput, SOFInfo* sof) {
  sof->prec = sofInput.getByte();
  sof->h = sofInput.getU16();
  sof->w = sofInput.getU16();
  sof->cps = sofInput.getByte();

  if (sof->prec < 2 || sof->prec > 16)
    ThrowRDE("Invalid precision (%u).", sof->prec);

  if (sof->h == 0 || sof->w == 0)
    ThrowRDE("Frame width or height set to zero");

  if (sof->cps > 4 || sof->cps < 1)
    ThrowRDE("Only from 1 to 4 components are supported.");

  if (sof->cps < mRaw->getCpp()) {
    ThrowRDE("Component count should be no less than sample count (%u vs %u).",
             sof->cps, mRaw->getCpp());
  }

  if (sof->cps > static_cast<uint32>(mRaw->dim.x)) {
    ThrowRDE("Component count should be no greater than row length (%u vs %u).",
             sof->cps, mRaw->dim.x);
  }

  if (sofInput.getRemainSize() != 3 * sof->cps)
    ThrowRDE("Header size mismatch.");

  for (uint32 i = 0; i < sof->cps; i++) {
    sof->compInfo[i].componentId = sofInput.getByte();

    uint32 subs = sofInput.getByte();
    frame.compInfo[i].superV = subs & 0xf;
    frame.compInfo[i].superH = subs >> 4;

    if (frame.compInfo[i].superV < 1 || frame.compInfo[i].superV > 4)
      ThrowRDE("Horizontal sampling factor is invalid.");

    if (frame.compInfo[i].superH < 1 || frame.compInfo[i].superH > 4)
      ThrowRDE("Horizontal sampling factor is invalid.");

    uint32 Tq = sofInput.getByte();
    if (Tq != 0)
      ThrowRDE("Quantized components not supported.");
  }

  sof->initialized = true;

  mRaw->metadata.subsampling.x = sof->compInfo[0].superH;
  mRaw->metadata.subsampling.y = sof->compInfo[0].superV;
}

void AbstractLJpegDecompressor::parseSOS(ByteStream sos) {
  assert(frame.initialized);

  if (sos.getRemainSize() != 1 + 2 * frame.cps + 3)
    ThrowRDE("Invalid SOS header length.");

  uint32 soscps = sos.getByte();
  if (frame.cps != soscps)
    ThrowRDE("Component number mismatch.");

  for (uint32 i = 0; i < frame.cps; i++) {
    uint32 cs = sos.getByte();
    uint32 td = sos.getByte() >> 4;

    if (td >= huff.size() || !huff[td])
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
  predictorMode = sos.getByte();
  // The spec says predictoreMode is in [0..7], but Hasselblad uses '8'.
  if (predictorMode > 8)
    ThrowRDE("Invalid predictor mode.");

  // Se + Ah Not used in LJPEG
  if (sos.getByte() != 0)
    ThrowRDE("Se/Ah not zero.");

  Pt = sos.getByte(); // Point Transform
  if (Pt > 15)
    ThrowRDE("Invalid Point transform.");

  decodeScan();
}

void AbstractLJpegDecompressor::parseDHT(ByteStream dht) {
  while (dht.getRemainSize() > 0) {
    uint32 b = dht.getByte();

    uint32 htClass = b >> 4;
    if (htClass != 0)
      ThrowRDE("Unsupported Table class.");

    uint32 htIndex = b & 0xf;
    if (htIndex >= huff.size())
      ThrowRDE("Invalid huffman table destination id.");

    if (huff[htIndex] != nullptr)
      ThrowRDE("Duplicate table definition");

    // copy 16 bytes from input stream to number of codes per length table
    uint32 nCodes = ht_.setNCodesPerLength(dht.getBuffer(16));

    // spec says 16 different codes is max but Hasselblad violates that -> 17
    if (nCodes > 17)
      ThrowRDE("Invalid DHT table.");

    // copy nCodes bytes from input stream to code values table
    ht_.setCodeValues(dht.getBuffer(nCodes));

    // see if we already have a HuffmanTable with the same codes
    for (const auto& i : huffmanTableStore)
      if (*i == ht_)
        huff[htIndex] = i.get();

    if (!huff[htIndex]) {
      // setup new ht_ and put it into the store
      auto dHT = std::make_unique<HuffmanTable>(ht_);
      dHT->setup(fullDecodeHT, fixDng16Bug);
      huff[htIndex] = dHT.get();
      huffmanTableStore.emplace_back(std::move(dHT));
    }
  }
}

JpegMarker AbstractLJpegDecompressor::getNextMarker(bool allowskip) {
  uchar8 c0;
  uchar8 c1 = input.getByte();
  do {
    c0 = c1;
    c1 = input.getByte();
  } while (allowskip && !(c0 == 0xFF && c1 != 0 && c1 != 0xFF));

  if (!(c0 == 0xFF && c1 != 0 && c1 != 0xFF))
    ThrowRDE("(Noskip) Expected marker not found. Propably corrupt file.");

  return static_cast<JpegMarker>(c1);
}

} // namespace rawspeed
