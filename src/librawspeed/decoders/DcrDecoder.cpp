/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real

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

#include "decoders/DcrDecoder.h"
#include "common/Common.h"                // for uint32, uchar8, ushort16
#include "common/NORangesSet.h"           // for NORangesSet
#include "decoders/RawDecoderException.h" // for RawDecoderException (ptr o...
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/ByteStream.h"                // for ByteStream
#include "io/IOException.h"               // for IOException
#include "tiff/TiffEntry.h"               // for TiffEntry, TiffDataType::T...
#include "tiff/TiffIFD.h"                 // for TiffRootIFD, TiffIFD
#include "tiff/TiffTag.h"                 // for TiffTag, TiffTag::COMPRESSION
#include <cassert>                        // for assert
#include <memory>                         // for unique_ptr
#include <string>                         // for operator==, string

using std::min;

namespace rawspeed {

class CameraMetaData;

bool DcrDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "Kodak";
}

void DcrDecoder::checkImageDimensions() {
  if (width > 4516 || height > 3012)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);
}

RawImage DcrDecoder::decodeRawInternal() {
  SimpleTiffDecoder::prepareForRawDecoding();

  ByteStream input(mFile, off);

  int compression = raw->getEntry(COMPRESSION)->getU32();
  if (65000 == compression) {
    TiffEntry *ifdoffset = mRootIFD->getEntryRecursive(KODAK_IFD);
    if (!ifdoffset)
      ThrowRDE("Couldn't find the Kodak IFD offset");

    NORangesSet<Buffer> ifds;

    assert(ifdoffset != nullptr);
    TiffRootIFD kodakifd(nullptr, &ifds, ifdoffset->getRootIfdData(),
                         ifdoffset->getU32());

    TiffEntry *linearization = kodakifd.getEntryRecursive(KODAK_LINEARIZATION);
    if (!linearization || linearization->count != 1024 ||
        linearization->type != TIFF_SHORT)
      ThrowRDE("Couldn't find the linearization table");

    assert(linearization != nullptr);
    auto linTable = linearization->getU16Array(1024);

    RawImageCurveGuard curveHandler(&mRaw, linTable, uncorrectedRawValues);

    // FIXME: dcraw does all sorts of crazy things besides this to fetch
    //        WB from what appear to be presets and calculate it in weird ways
    //        The only file I have only uses this method, if anybody careas look
    //        in dcraw.c parse_kodak_ifd() for all that weirdness
    TiffEntry* blob = kodakifd.getEntryRecursive(static_cast<TiffTag>(0x03fd));
    if (blob && blob->count == 72) {
      mRaw->metadata.wbCoeffs[0] = 2048.0F / blob->getU16(20);
      mRaw->metadata.wbCoeffs[1] = 2048.0F / blob->getU16(21);
      mRaw->metadata.wbCoeffs[2] = 2048.0F / blob->getU16(22);
    }

    decodeKodak65000(&input, width, height);
  } else
    ThrowRDE("Unsupported compression %d", compression);

  return mRaw;
}

void DcrDecoder::decodeKodak65000(ByteStream* input, uint32 w, uint32 h) {
  ushort16 buf[256];
  uint32 pred[2];
  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;

  uint32 random = 0;
  for (uint32 y = 0; y < h; y++) {
    auto* dest = reinterpret_cast<ushort16*>(&data[y * pitch]);
    for (uint32 x = 0 ; x < w; x += 256) {
      pred[0] = pred[1] = 0;
      uint32 len = min(256U, w - x);
      decodeKodak65000Segment(input, buf, len);
      for (uint32 i = 0; i < len; i++) {
        pred[i & 1] += buf[i];

        ushort16 value = pred[i & 1];
        if (value > 1023)
          ThrowRDE("Value out of bounds %d", value);
        if(uncorrectedRawValues)
          dest[x+i] = value;
        else
          mRaw->setWithLookUp(value, reinterpret_cast<uchar8*>(&dest[x + i]),
                              &random);
      }
    }
  }
}

void DcrDecoder::decodeKodak65000Segment(ByteStream* input, ushort16* out,
                                         uint32 bsize) {
  uchar8 blen[768];
  uint64 bitbuf=0;
  uint32 bits=0;

  bsize = (bsize + 3) & -4;
  for (uint32 i=0; i < bsize; i+=2) {
    blen[i] = input->peekByte() & 15;
    blen[i + 1] = input->getByte() >> 4;
  }
  if ((bsize & 7) == 4) {
    bitbuf = (static_cast<uint64>(input->getByte())) << 8UL;
    bitbuf += (static_cast<int>(input->getByte()));
    bits = 16;
  }
  for (uint32 i=0; i < bsize; i++) {
    uint32 len = blen[i];
    if (bits < len) {
      for (uint32 j=0; j < 32; j+=8) {
        bitbuf += static_cast<long long>(static_cast<int>(input->getByte()))
                  << (bits + (j ^ 8));
      }
      bits += 32;
    }
    uint32 diff = static_cast<uint32>(bitbuf) & (0xffff >> (16 - len));
    bitbuf >>= len;
    bits -= len;
    diff = len != 0 ? HuffmanTable::signExtended(diff, len) : diff;
    out[i] = diff;
  }
}

void DcrDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, "", 0);
}

} // namespace rawspeed
