#include "StdAfx.h"
#include "LJpegDecompressor.h"

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

    http://www.klauspost.com
*/

namespace RawSpeed {

LJpegDecompressor::LJpegDecompressor(FileMap* file, RawImage img):
    mFile(file), mRaw(img) {
  input = 0;
  skipX = skipY = 0;
  mDNGCompatible = false;
  slicesW.clear();
  mUseBigtable = true;
  mCanonFlipDim = false;
  mCanonDoubleHeight = false;
}

LJpegDecompressor::~LJpegDecompressor(void) {
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
  } catch (IOException) {
    ThrowRDE("LJpegDecompressor: IO exception, read outside file. Corrupt File.");
  }
}

void LJpegDecompressor::startDecoder(uint32 offset, uint32 size, uint32 offsetX, uint32 offsetY) {
  if (!mFile->isValid(offset, size))
    ThrowRDE("LJpegDecompressor::startDecoder: Start offset plus size is longer than file. Truncated file.");
  if ((int)offsetX >= mRaw->dim.x)
    ThrowRDE("LJpegDecompressor::startDecoder: X offset outside of image");
  if ((int)offsetY >= mRaw->dim.y)
    ThrowRDE("LJpegDecompressor::startDecoder: Y offset outside of image");
  offX = offsetX;
  offY = offsetY;

  try {
    // JPEG is big endian
    input = new ByteStream(mFile, offset, size, getHostEndianness() == big);

    if (getNextMarker(false) != M_SOI)
      ThrowRDE("LJpegDecompressor::startDecoder: Image did not start with SOI. Probably not an LJPEG");
//    _RPT0(0,"Found SOI marker\n");

    bool moreImage = true;
    while (moreImage) {
      JpegMarker m = getNextMarker(true);

      switch (m) {
        case M_SOS:
//          _RPT0(0,"Found SOS marker\n");
          parseSOS();
          break;
        case M_EOI:
//          _RPT0(0,"Found EOI marker\n");
          moreImage = false;
          break;

        case M_DHT:
//          _RPT0(0,"Found DHT marker\n");
          parseDHT();
          break;

        case M_DQT:
          ThrowRDE("LJpegDecompressor: Not a valid RAW file.");
          break;

        case M_DRI:
//          _RPT0(0,"Found DRI marker\n");
          break;

        case M_APP0:
//          _RPT0(0,"Found APP0 marker\n");
          break;

        case M_SOF3:
//          _RPT0(0,"Found SOF 3 marker:\n");
          parseSOF(&frame);
          break;

        default:  // Just let it skip to next marker
          _RPT1(0, "Found marker:0x%x. Skipping\n", (int)m);
          break;
      }
    }

  } catch (IOException) {
    throw;
  }
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
  uint32 soscps = input->getByte();
  if (frame.cps != soscps)
    ThrowRDE("LJpegDecompressor::parseSOS: Component number mismatch.");

  for (uint32 i = 0;i < frame.cps;i++) {
    uint32 cs = input->getByte();

    uint32 count = 0;  // Find the correct component
    while (frame.compInfo[count].componentId != cs) {
      if (count >= frame.cps)
        ThrowRDE("LJpegDecompressor::parseSOS: Invalid Component Selector");
      count++;
    }

    uint32 b = input->getByte();
    uint32 td = b >> 4;
    if (td > 3)
      ThrowRDE("LJpegDecompressor::parseSOS: Invalid Huffman table selection");
    if (!huff[td])
      ThrowRDE("LJpegDecompressor::parseSOS: Invalid Huffman table selection, not defined.");

    if (count > 3)
      ThrowRDE("LJpegDecompressor::parseSOS: Component count out of range");

    frame.compInfo[count].dcTblNo = td;
  }

  // Get predictor
  pred = input->getByte();
  if (pred > 7)
    ThrowRDE("LJpegDecompressor::parseSOS: Invalid predictor mode.");

  input->skipBytes(1);                    // Se + Ah Not used in LJPEG
  uint32 b = input->getByte();
  Pt = b & 0xf;        // Point Transform

  uint32 cheadersize = 3 + frame.cps * 2 + 3;
  _ASSERTE(cheadersize == headerLength);

  bits = new BitPumpJPEG(*input);
  try {
    decodeScan();
  } catch (...) {
    delete bits;
    throw;
  }
  input->skipBytes(bits->getBufferPosition());
  delete bits;
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
      ht->setup(mUseBigtable, mDNGCompatible);
      huff[htIndex] = ht.get();
      huffmanTableStore.emplace_back(std::move(ht));
    }
    headerLength -= 1 + 16 + nCodes;
  }
}


JpegMarker LJpegDecompressor::getNextMarker(bool allowskip) {

  if (!allowskip) {
    uchar8 id = input->getByte();
    if (id != 0xff)
      ThrowRDE("LJpegDecompressor::getNextMarker: (Noskip) Expected marker not found. Propably corrupt file.");

    JpegMarker mark = (JpegMarker)input->getByte();

    if (M_FILL == mark || M_STUFF == mark)
      ThrowRDE("LJpegDecompressor::getNextMarker: (Noskip) Expected marker, but found stuffed 00 or ff.");

    return mark;
  } else {
    // skipToMarker
    uchar8 c0, c1 = input->getByte();
    do {
      c0 = c1;
      c1 = input->getByte();
    } while (!(c0 == 0xFF && c1 != 0 && c1 != 0xFF));

    return (JpegMarker)c1;
  }
}

} // namespace RawSpeed
