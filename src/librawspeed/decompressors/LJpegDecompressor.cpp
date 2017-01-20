#include "common/StdAfx.h"
#include "decompressors/LJpegDecompressor.h"

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

namespace RawSpeed {

LJpegDecompressor::~LJpegDecompressor() {
  delete input;
}

void LJpegDecompressor::getSOF(SOFInfo* sof, uint32 offset, uint32 size) {
  if (!mFile->isValid(offset, size))
    ThrowRDE("LJpegDecompressor::getSOF: Start offset plus size is longer than file. Truncated file.");
  try {
    // JPEG is big endian
    input = new ByteStream(mFile, offset, size, getHostEndianness() == big);

    if (getNextMarker(false) != M_SOI)
      ThrowRDE("LJpegDecompressor::getSOF: Image did not start with SOI. Probably not an LJPEG");

    while (true) {
      JpegMarker m = getNextMarker(true);
      if (M_SOF3 == m) {
        parseSOF(sof);
        return;
      }
      if (M_EOI == m) {
        ThrowRDE("LJpegDecompressor: Could not locate Start of Frame.");
        return;
      }
    }
  } catch (IOException &) {
    ThrowRDE("LJpegDecompressor: IO exception, read outside file. Corrupt File.");
  }
}

void LJpegDecompressor::decode(uint32 offset, uint32 size, uint32 offsetX, uint32 offsetY) {
  if (!mFile->isValid(offset, size))
    ThrowRDE("LJpegDecompressor: Start offset plus size is longer than file. Truncated file.");
  if ((int)offsetX >= mRaw->dim.x)
    ThrowRDE("LJpegDecompressor: X offset outside of image");
  if ((int)offsetY >= mRaw->dim.y)
    ThrowRDE("LJpegDecompressor: Y offset outside of image");
  offX = offsetX;
  offY = offsetY;

  // JPEG is big endian
  input = new ByteStream(mFile, offset, size, getHostEndianness() == big);

  if (getNextMarker(false) != M_SOI)
    ThrowRDE("LJpegDecompressor: Image did not start with SOI. Probably not an LJPEG");

  JpegMarker m;
  do {
    m = getNextMarker(true);

    switch (m) {
    case M_DHT:  parseDHT(); break;
    case M_SOF3: parseSOF(&frame); break;
    case M_SOS:  parseSOS(); break;
    case M_DQT:  ThrowRDE("LJpegDecompressor: Not a valid RAW file.");
    default:  // Just let it skip to next marker
      break;
    }
  } while (m != M_EOI);
}

void LJpegDecompressor::parseSOF(SOFInfo* sof) {
  uint32 headerLength = input->getShort();
  sof->prec = input->getByte();
  sof->h = input->getShort();
  sof->w = input->getShort();
  sof->cps = input->getByte();

  if (sof->prec > 16)
    ThrowRDE("LJpegDecompressor: More than 16 bits per channel is not supported.");

  if (sof->cps > 4 || sof->cps < 1)
    ThrowRDE("LJpegDecompressor: Only from 1 to 4 components are supported.");

  if (headerLength != 8 + sof->cps*3)
    ThrowRDE("LJpegDecompressor: Header size mismatch.");

  for (uint32 i = 0; i < sof->cps; i++) {
    sof->compInfo[i].componentId = input->getByte();
    uint32 subs = input->getByte();
    frame.compInfo[i].superV = subs & 0xf;
    frame.compInfo[i].superH = subs >> 4;
    uint32 Tq = input->getByte();
    if (Tq != 0)
      ThrowRDE("LJpegDecompressor: Quantized components not supported.");
  }
  sof->initialized = true;
}

void LJpegDecompressor::parseSOS() {
  if (!frame.initialized)
    ThrowRDE("LJpegDecompressor::parseSOS: Frame not yet initialized (SOF Marker not parsed)");

  uint32 headerLength = input->getShort();
  if (headerLength != 3 + frame.cps * 2 + 3)
    ThrowRDE("LJpegDecompressor::parseSOS: Invalid SOS header length.");

  uint32 soscps = input->getByte();
  if (frame.cps != soscps)
    ThrowRDE("LJpegDecompressor::parseSOS: Component number mismatch.");

  for (uint32 i = 0; i < frame.cps; i++) {
    uint32 cs = input->getByte();
    uint32 td = input->getByte() >> 4;

    if (td > 3 || !huff[td])
      ThrowRDE("LJpegDecompressor::parseSOS: Invalid Huffman table selection.");

    int ciIndex = -1;
    for (uint32 j = 0; j < frame.cps; ++j) {
      if (frame.compInfo[j].componentId == cs)
        ciIndex = j;
    }

    if (ciIndex == -1)
        ThrowRDE("LJpegDecompressor::parseSOS: Invalid Component Selector");

    frame.compInfo[ciIndex].dcTblNo = td;
  }

  // Get predictor
  pred = input->getByte();
  if (pred > 8)
    ThrowRDE("LJpegDecompressor::parseSOS: Invalid predictor mode.");

  input->skipBytes(1);         // Se + Ah Not used in LJPEG
  Pt = input->getByte() & 0xf; // Point Transform

  decodeScan();
}

void LJpegDecompressor::parseDHT() {
  uint32 headerLength = input->getShort() - 2; // Subtract myself

  while (headerLength)  {
    uint32 b = input->getByte();

    uint32 htClass = b >> 4;
    if (htClass != 0)
      ThrowRDE("LJpegDecompressor::parseDHT: Unsupported Table class.");

    uint32 htIndex = b & 0xf;
    if (htIndex >= huff.size())
      ThrowRDE("LJpegDecompressor::parseDHT: Invalid huffman table destination id.");

    if (huff[htIndex] != nullptr)
      ThrowRDE("LJpegDecompressor::parseDHT: Duplicate table definition");

    auto ht = make_unique<HuffmanTable>();

    // copy 16 bytes from input stream to number of codes per length table
    uint32 nCodes = ht->setNCodesPerLength(input->getBuffer(16));
    // spec says 16 different codes is max but Hasselblad violates that -> 17
    if (nCodes > 17 || headerLength < 1 + 16 + nCodes)
      ThrowRDE("LJpegDecompressor::parseDHT: Invalid DHT table.");

    // copy nCodes bytes from input stream to code values table
    ht->setCodeValues(input->getBuffer(nCodes));

    // see if we already have a HuffmanTable with the same codes
    for (const auto& i : huffmanTableStore)
      if (*i == *ht)
        huff[htIndex] = i.get();

    if (!huff[htIndex]) {
      // setup new ht and put it into the store
      ht->setup(mFullDecodeHT, mDNGCompatible);
      huff[htIndex] = ht.get();
      huffmanTableStore.emplace_back(std::move(ht));
    }
    headerLength -= 1 + 16 + nCodes;
  }
}

JpegMarker LJpegDecompressor::getNextMarker(bool allowskip) {
  uchar8 c0, c1 = input->getByte();
  do {
    c0 = c1;
    c1 = input->getByte();
  } while (allowskip && !(c0 == 0xFF && c1 != 0 && c1 != 0xFF));

  if (!(c0 == 0xFF && c1 != 0 && c1 != 0xFF))
    ThrowRDE("LJpegDecompressor::getNextMarker: (Noskip) Expected marker not found. Propably corrupt file.");

  return (JpegMarker)c1;
}

} // namespace RawSpeed
