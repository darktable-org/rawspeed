/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real

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

#include "decoders/OrfDecoder.h"
#include "common/Common.h"                          // for uint32, ushort16
#include "common/Point.h"                           // for iPoint2D
#include "decoders/RawDecoderException.h"           // for RawDecoderExcept...
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/BitPumpMSB.h"                          // for BitPumpMSB
#include "io/Buffer.h"                              // for Buffer
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for Endianness
#include "io/IOException.h"                         // for IOException
#include "metadata/Camera.h"                        // for Hints
#include "metadata/ColorFilterArray.h"              // for ColorFilterArray
#include "parsers/TiffParserException.h"            // for TiffParserException
#include "tiff/TiffEntry.h"                         // for TiffEntry
#include "tiff/TiffIFD.h"                           // for TiffRootIFD, Tif...
#include "tiff/TiffTag.h"                           // for TiffTag, TiffTag...
#include <algorithm>                                // for min
#include <cmath>                                    // for signbit
#include <cstdlib>                                  // for abs
#include <cstring>                                  // for memset
#include <memory>                                   // for unique_ptr

using namespace std;

namespace RawSpeed {

class CameraMetaData;

RawImage OrfDecoder::decodeRawInternal() {
  auto raw = mRootIFD->getIFDWithTag(STRIPOFFSETS);

  int compression = raw->getEntry(COMPRESSION)->getU32();
  if (1 != compression)
    ThrowRDE("Unsupported compression");

  TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);

  if (counts->count != offsets->count) {
    ThrowRDE(
        "Byte count number does not match strip size: count:%u, strips:%u ",
        counts->count, offsets->count);
  }

  //TODO: this code assumes that all strips are layed out directly after another without padding and in order
  uint32 off = raw->getEntry(STRIPOFFSETS)->getU32();
  uint32 size = 0;
  for (uint32 i=0; i < counts->count; i++)
    size += counts->getU32(i);

  if (!mFile->isValid(off, size))
    ThrowRDE("Truncated file");

  uint32 width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32 height = raw->getEntry(IMAGELENGTH)->getU32();

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  ByteStream input(offsets->getRootIfdData());
  input.setPosition(off);

  try {
    if (offsets->count != 1 || hints.has("force_uncompressed"))
      decodeUncompressed(input, width, height, size);
    else
      decodeCompressed(input, width, height);
  } catch (IOException &e) {
     mRaw->setError(e.what());
  }

  return mRaw;
}

void OrfDecoder::decodeUncompressed(ByteStream& s, uint32 w, uint32 h, uint32 size) {
  UncompressedDecompressor u(s, mRaw, uncorrectedRawValues);
  if (hints.has("packed_with_control"))
    u.decode12BitRaw<little, true>(w, h);
  else if (hints.has("jpeg32_bitorder")) {
    iPoint2D dimensions(w, h), pos(0, 0);
    u.readUncompressedRaw(dimensions, pos, w * 12 / 8, 12, BitOrder_Jpeg32);
  } else if (size >= w*h*2) { // We're in an unpacked raw
    if (s.isInNativeByteOrder())
      u.decodeRawUnpacked<12, little>(w, h);
    else
      u.decode12BitRawUnpackedLeftAligned<big>(w, h);
  } else if (size >= w*h*3/2) { // We're in one of those weird interlaced packed raws
    u.decode12BitRawInterlaced<big>(w, h);
  } else {
    ThrowRDE("Don't know how to handle the encoding in this file");
  }
}

/* This is probably the slowest decoder of them all.
 * I cannot see any way to effectively speed up the prediction
 * phase, which is by far the slowest part of this algorithm.
 * Also there is no way to multithread this code, since prediction
 * is based on the output of all previous pixel (bar the first four)
 */

void OrfDecoder::decodeCompressed(ByteStream& s, uint32 w, uint32 h) {
  int nbits, sign, low, high, i, left0, nw0, left1, nw1;
  int acarry0[3], acarry1[3], pred, diff;

  uchar8* data = mRaw->getData();
  int pitch = mRaw->pitch;

  /* Build a table to quickly look up "high" value */
  unique_ptr<char[]> bittable(new char[4096]);

  for (i = 0; i < 4096; i++) {
    int b = i;
    for (high = 0; high < 12; high++)
      if ((b>>(11-high))&1)
        break;
    bittable[i] = min(12,high);
  }
  left0 = nw0 = left1 = nw1 = 0;

  s.skipBytes(7);
  BitPumpMSB bits(s);

  for (uint32 y = 0; y < h; y++) {
    memset(acarry0, 0, sizeof acarry0);
    memset(acarry1, 0, sizeof acarry1);
    auto *dest = (ushort16 *)&data[y * pitch];
    bool y_border = y < 2;
    bool border = true;
    for (uint32 x = 0; x < w; x++) {
      bits.fill();
      i = 2 * (acarry0[2] < 3);
      for (nbits = 2 + i; (ushort16) acarry0[0] >> (nbits + i); nbits++);

      int b = bits.peekBitsNoFill(15);
      sign = (b >> 14) * -1;
      low  = (b >> 12) & 3;
      high = bittable[b&4095];

      // Skip bytes used above or read bits
      if (high == 12) {
        bits.skipBitsNoFill(15);
        high = bits.getBits(16 - nbits) >> 1;
      } else {
        bits.skipBitsNoFill(high + 1 + 3);
      }

      acarry0[0] = (high << nbits) | bits.getBits(nbits);
      diff = (acarry0[0] ^ sign) + acarry0[1];
      acarry0[1] = (diff * 3 + acarry0[1]) >> 5;
      acarry0[2] = acarry0[0] > 16 ? 0 : acarry0[2] + 1;

      if (border) {
        if (y_border && x < 2)
          pred = 0;
        else {
          if (y_border)
            pred = left0;
          else {
            pred = dest[-pitch + ((int)x)];
            nw0 = pred;
          }
        }
        dest[x] = pred + ((diff * 4) | low);
        // Set predictor
        left0 = dest[x];
      } else {
        // Have local variables for values used several tiles
        // (having a "ushort16 *dst_up" that caches dest[-pitch+((int)x)] is actually slower, probably stack spill or aliasing)
        int up  = dest[-pitch+((int)x)];
        int leftMinusNw = left0 - nw0;
        int upMinusNw = up - nw0;
        // Check if sign is different, and they are both not zero
        if ((signbit(leftMinusNw) ^ signbit(upMinusNw)) &&
            (leftMinusNw != 0 && upMinusNw != 0)) {
          if (abs(leftMinusNw) > 32 || abs(upMinusNw) > 32)
            pred = left0 + upMinusNw;
          else
            pred = (left0 + up) >> 1;
        } else
          pred = abs(leftMinusNw) > abs(upMinusNw) ? left0 : up;

        dest[x] = pred + ((diff * 4) | low);
        // Set predictors
        left0 = dest[x];
        nw0 = up;
      }

      // ODD PIXELS
      x += 1;
      bits.fill();
      i = 2 * (acarry1[2] < 3);
      for (nbits = 2 + i; (ushort16) acarry1[0] >> (nbits + i); nbits++);
      b = bits.peekBitsNoFill(15);
      sign = (b >> 14) * -1;
      low  = (b >> 12) & 3;
      high = bittable[b&4095];

      // Skip bytes used above or read bits
      if (high == 12) {
        bits.skipBitsNoFill(15);
        high = bits.getBits(16 - nbits) >> 1;
      } else {
        bits.skipBitsNoFill(high + 1 + 3);
      }

      acarry1[0] = (high << nbits) | bits.getBits(nbits);
      diff = (acarry1[0] ^ sign) + acarry1[1];
      acarry1[1] = (diff * 3 + acarry1[1]) >> 5;
      acarry1[2] = acarry1[0] > 16 ? 0 : acarry1[2] + 1;

      if (border) {
        if (y_border && x < 2)
          pred = 0;
        else {
          if (y_border)
            pred = left1;
          else {
            pred = dest[-pitch + ((int)x)];
            nw1 = pred;
          }
        }
        dest[x] = left1 = pred + ((diff * 4) | low);
      } else {
        int up  = dest[-pitch+((int)x)];
        int leftMinusNw = left1 - nw1;
        int upMinusNw = up - nw1;

        // Check if sign is different, and they are both not zero
        if ((signbit(leftMinusNw) ^ signbit(upMinusNw)) &&
            (leftMinusNw != 0 && upMinusNw != 0)) {
          if (abs(leftMinusNw) > 32 || abs(upMinusNw) > 32)
            pred = left1 + upMinusNw;
          else
            pred = (left1 + up) >> 1;
        } else
          pred = abs(leftMinusNw) > abs(upMinusNw) ? left1 : up;

        dest[x] = left1 = pred + ((diff * 4) | low);
        nw1 = up;
      }
      border = y_border;
    }
  }
}

void OrfDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  int iso = 0;
  mRaw->cfa.setCFA(iPoint2D(2,2), CFA_RED, CFA_GREEN, CFA_GREEN, CFA_BLUE);

  if (mRootIFD->hasEntryRecursive(ISOSPEEDRATINGS))
    iso = mRootIFD->getEntryRecursive(ISOSPEEDRATINGS)->getU32();

  setMetaData(meta, "", iso);

  if (mRootIFD->hasEntryRecursive(OLYMPUSREDMULTIPLIER) &&
      mRootIFD->hasEntryRecursive(OLYMPUSBLUEMULTIPLIER)) {
    mRaw->metadata.wbCoeffs[0] = (float) mRootIFD->getEntryRecursive(OLYMPUSREDMULTIPLIER)->getU16();
    mRaw->metadata.wbCoeffs[1] = 256.0f;
    mRaw->metadata.wbCoeffs[2] = (float) mRootIFD->getEntryRecursive(OLYMPUSBLUEMULTIPLIER)->getU16();
  } else {
    // Newer cameras process the Image Processing SubIFD in the makernote
    if(mRootIFD->hasEntryRecursive(OLYMPUSIMAGEPROCESSING)) {
      TiffEntry *img_entry = mRootIFD->getEntryRecursive(OLYMPUSIMAGEPROCESSING);
      try {
        // get makernote ifd with containing Buffer
        TiffRootIFD image_processing(nullptr, img_entry->getRootIfdData(),
                                     img_entry->getU32());

        // Get the WB
        if(image_processing.hasEntry((TiffTag) 0x0100)) {
          TiffEntry *wb = image_processing.getEntry((TiffTag) 0x0100);
          if (wb->count == 2 || wb->count == 4) {
            mRaw->metadata.wbCoeffs[0] = wb->getFloat(0);
            mRaw->metadata.wbCoeffs[1] = 256.0f;
            mRaw->metadata.wbCoeffs[2] = wb->getFloat(1);
          }
        }

        // Get the black levels
        if(image_processing.hasEntry((TiffTag) 0x0600)) {
          TiffEntry *blackEntry = image_processing.getEntry((TiffTag) 0x0600);
          // Order is assumed to be RGGB
          if (blackEntry->count == 4) {
            for (int i = 0; i < 4; i++) {
              auto c = mRaw->cfa.getColorAt(i & 1, i >> 1);
              int j;
              switch (c) {
              case CFA_RED:
                j = 0;
                break;
              case CFA_GREEN:
                j = i < 2 ? 1 : 2;
                break;
              case CFA_BLUE:
                j = 3;
                break;
              default:
                ThrowRDE("Unexpected CFA color: %u", c);
              }

              mRaw->blackLevelSeparate[i] = blackEntry->getU16(j);
            }
            // Adjust whitelevel based on the read black (we assume the dynamic range is the same)
            mRaw->whitePoint -= (mRaw->blackLevel - mRaw->blackLevelSeparate[0]);
          }
        }
      } catch (TiffParserException &e) {
        mRaw->setError(e.what());
      }
    }
  }
}

} // namespace RawSpeed
