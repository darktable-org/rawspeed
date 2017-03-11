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
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for RawDecoderException (ptr o...
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/Buffer.h"                    // for Buffer
#include "io/ByteStream.h"                // for ByteStream
#include "io/IOException.h"               // for IOException
#include "tiff/TiffEntry.h"               // for TiffEntry, TiffDataType::T...
#include "tiff/TiffIFD.h"                 // for TiffRootIFD, TiffIFD
#include "tiff/TiffTag.h"                 // for TiffTag, TiffTag::CFAPATTERN
#include <memory>                         // for unique_ptr
#include <vector>                         // for vector

using namespace std;

namespace RawSpeed {

class CameraMetaData;

RawImage DcrDecoder::decodeRawInternal() {
  auto raw = mRootIFD->getIFDWithTag(CFAPATTERN);
  uint32 width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32 height = raw->getEntry(IMAGELENGTH)->getU32();
  uint32 off = raw->getEntry(STRIPOFFSETS)->getU32();
  uint32 c2 = raw->getEntry(STRIPBYTECOUNTS)->getU32();

  if (off > mFile->getSize())
    ThrowRDE("Offset is out of bounds");

  if (c2 > mFile->getSize() - off) {
    mRaw->setError("Warning: byte count larger than file size, file probably truncated.");
  }

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();
  ByteStream input(mFile, off);

  int compression = raw->getEntry(COMPRESSION)->getU32();
  if (65000 == compression) {
    TiffEntry *ifdoffset = mRootIFD->getEntryRecursive(KODAK_IFD);
    if (!ifdoffset)
      ThrowRDE("Couldn't find the Kodak IFD offset");
    TiffRootIFD kodakifd(nullptr, ifdoffset->getRootIfdData(),
                         ifdoffset->getU32());
    TiffEntry *linearization = kodakifd.getEntryRecursive(KODAK_LINEARIZATION);
    if (!linearization || linearization->count != 1024 || linearization->type != TIFF_SHORT) {
      ThrowRDE("Couldn't find the linearization table");
    }

    auto linTable = linearization->getU16Array(1024);

    if (!uncorrectedRawValues)
      mRaw->setTable(linTable.data(), linTable.size(), true);

    // FIXME: dcraw does all sorts of crazy things besides this to fetch
    //        WB from what appear to be presets and calculate it in weird ways
    //        The only file I have only uses this method, if anybody careas look
    //        in dcraw.c parse_kodak_ifd() for all that weirdness
    TiffEntry *blob = kodakifd.getEntryRecursive((TiffTag) 0x03fd);
    if (blob && blob->count == 72) {
      mRaw->metadata.wbCoeffs[0] = (float) 2048.0f / blob->getU16(20);
      mRaw->metadata.wbCoeffs[1] = (float) 2048.0f / blob->getU16(21);
      mRaw->metadata.wbCoeffs[2] = (float) 2048.0f / blob->getU16(22);
    }

    try {
      decodeKodak65000(input, width, height);
    } catch (IOException &) {
      mRaw->setError("IO error occurred while reading image. Returning partial result.");
    }

    // Set the table, if it should be needed later.
    if (uncorrectedRawValues) {
      mRaw->setTable(linTable.data(), linTable.size(), false);
    } else {
      mRaw->setTable(nullptr);
    }
  } else
    ThrowRDE("Unsupported compression %d", compression);

  return mRaw;
}

void DcrDecoder::decodeKodak65000(ByteStream &input, uint32 w, uint32 h) {
  ushort16 buf[256];
  uint32 pred[2];
  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;

  uint32 random = 0;
  for (uint32 y = 0; y < h; y++) {
    auto *dest = (ushort16 *)&data[y * pitch];
    for (uint32 x = 0 ; x < w; x += 256) {
      pred[0] = pred[1] = 0;
      uint32 len = min(256u, w - x);
      decodeKodak65000Segment(input, buf, len);
      for (uint32 i = 0; i < len; i++) {
        ushort16 value = pred[i & 1] += buf[i];
        if (value > 1023)
          ThrowRDE("Value out of bounds %d", value);
        if(uncorrectedRawValues)
          dest[x+i] = value;
        else
          mRaw->setWithLookUp(value, (uchar8*)&dest[x+i], &random);
      }
    }
  }
}

void DcrDecoder::decodeKodak65000Segment(ByteStream &input, ushort16 *out, uint32 bsize) {
  uchar8 blen[768];
  uint64 bitbuf=0;
  uint32 bits=0;

  bsize = (bsize + 3) & -4;
  for (uint32 i=0; i < bsize; i+=2) {
    blen[i] = input.peekByte() & 15;
    blen[i+1] = input.getByte() >> 4;
  }
  if ((bsize & 7) == 4) {
    bitbuf = ((uint64)input.getByte()) << 8UL;
    bitbuf += ((int) input.getByte());
    bits = 16;
  }
  for (uint32 i=0; i < bsize; i++) {
    uint32 len = blen[i];
    if (bits < len) {
      for (uint32 j=0; j < 32; j+=8) {
        bitbuf += (long long) ((int) input.getByte()) << (bits+(j^8));
      }
      bits += 32;
    }
    uint32 diff = (uint32)bitbuf & (0xffff >> (16-len));
    bitbuf >>= len;
    bits -= len;
    diff = len ? HuffmanTable::signExtended(diff, len) : diff;
    out[i] = diff;
  }
}

void DcrDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, "", 0);
}

} // namespace RawSpeed
