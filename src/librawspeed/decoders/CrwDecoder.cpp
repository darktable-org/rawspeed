/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2015 Roman Lebedev

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

#include "decoders/CrwDecoder.h"
#include "common/Common.h"                // for ushort16, uint32, uchar8
#include "common/Memory.h"                // for alignedFree, alignedMalloc...
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/ByteStream.h"                // for ByteStream
#include "metadata/Camera.h"              // for Hints
#include "metadata/ColorFilterArray.h"    // for CFAColor::CFA_GREEN, CFACo...
#include "tiff/CiffEntry.h"               // for CiffEntry, CiffDataType::C...
#include "tiff/CiffIFD.h"                 // for CiffIFD
#include "tiff/CiffTag.h"                 // for CiffTag, CiffTag::CIFF_MAK...
#include <algorithm>                      // for min
#include <cmath>                          // for copysignf, expf, logf
#include <cstdio>                         // for fprintf, stderr
#include <cstdlib>                        // for abs
#include <cstring>                        // for memset
#include <exception>                      // for exception
#include <string>                         // for string
#include <vector>                         // for vector

using namespace std;

namespace RawSpeed {

class CameraMetaData;

CrwDecoder::CrwDecoder(CiffIFD *rootIFD, FileMap* file) :
    RawDecoder(file), mRootIFD(rootIFD) {
  mHuff[0] = nullptr;
  mHuff[1] = nullptr;
}

CrwDecoder::~CrwDecoder() {
  if (mRootIFD)
    delete mRootIFD;
  mRootIFD = nullptr;
  if (mHuff[0] != nullptr)
    alignedFree(mHuff[0]);
  if (mHuff[1] != nullptr)
    alignedFree(mHuff[1]);
  mHuff[0] = nullptr;
  mHuff[1] = nullptr;
}

RawImage CrwDecoder::decodeRawInternal() {
  CiffEntry *sensorInfo = mRootIFD->getEntryRecursive(CIFF_SENSORINFO);

  if (!sensorInfo || sensorInfo->count < 6 || sensorInfo->type != CIFF_SHORT)
    ThrowRDE("CRW: Couldn't find image sensor info");

  uint32 width = sensorInfo->getU16(1);
  uint32 height = sensorInfo->getU16(2);

  CiffEntry *decTable = mRootIFD->getEntryRecursive(CIFF_DECODERTABLE);
  if (!decTable || decTable->type != CIFF_LONG)
    ThrowRDE("CRW: Couldn't find decoder table");

  uint32 dec_table = decTable->getU32();
  if (dec_table > 2)
    ThrowRDE("CRW: Unknown decoder table %d", dec_table);

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  bool lowbits = ! hints.has("no_decompressed_lowbits");
  decodeRaw(lowbits, dec_table, width, height);

  return mRaw;
}

void CrwDecoder::checkSupportInternal(CameraMetaData *meta) {
  vector<CiffIFD*> data = mRootIFD->getIFDsWithTag(CIFF_MAKEMODEL);
  if (data.empty())
    ThrowRDE("CRW Support check: Model name not found");
  vector<string> makemodel = data[0]->getEntry(CIFF_MAKEMODEL)->getStrings();
  if (makemodel.size() < 2)
    ThrowRDE("CRW Support check: wrong number of strings for make/model");
  string make = makemodel[0];
  string model = makemodel[1];

  this->checkCameraSupported(meta, make, model, "");
}

// based on exiftool's Image::ExifTool::Canon::CanonEv
static float canonEv(const long in) {
  // remove sign
  long val = abs(in);
  // remove fraction
  auto frac = static_cast<float>(val & 0x1f);
  val -= long(frac);
  // convert 1/3 (0x0c) and 2/3 (0x14) codes
  if (frac == 0x0c) {
    frac = 32.0f / 3;
  }
  else if (frac == 0x14) {
    frac = 64.0f / 3;
  }
  return copysignf((val + frac) / 32.0f, in);
}

void CrwDecoder::decodeMetaDataInternal(CameraMetaData *meta) {
  int iso = 0;
  mRaw->cfa.setCFA(iPoint2D(2,2), CFA_RED, CFA_GREEN, CFA_GREEN, CFA_BLUE);
  vector<CiffIFD*> data = mRootIFD->getIFDsWithTag(CIFF_MAKEMODEL);
  if (data.empty())
    ThrowRDE("CRW Support check: Model name not found");
  vector<string> makemodel = data[0]->getEntry(CIFF_MAKEMODEL)->getStrings();
  if (makemodel.size() < 2)
    ThrowRDE("CRW Support check: wrong number of strings for make/model");
  string make = makemodel[0];
  string model = makemodel[1];
  string mode;

  if (mRootIFD->hasEntryRecursive(CIFF_SHOTINFO)) {
    CiffEntry *shot_info = mRootIFD->getEntryRecursive(CIFF_SHOTINFO);
    if (shot_info->type == CIFF_SHORT && shot_info->count >= 2) {
      // os << exp(canonEv(value.toLong()) * log(2.0)) * 100.0 / 32.0;
      ushort16 iso_index = shot_info->getU16(2);
      iso = expf(canonEv((long)iso_index) * logf(2.0)) * 100.0f / 32.0f;
    }
  }

  // Fetch the white balance
  try{
    if(mRootIFD->hasEntryRecursive((CiffTag)0x0032)) {
      CiffEntry *wb = mRootIFD->getEntryRecursive((CiffTag)0x0032);
      if (wb->type == CIFF_BYTE && wb->count == 768) {
        // We're in a D30 file, values are RGGB
        // This will probably not get used anyway as a 0x102c tag should exist
        mRaw->metadata.wbCoeffs[0] = (float) (1024.0 /wb->getByte(72));
        mRaw->metadata.wbCoeffs[1] = (float) ((1024.0/wb->getByte(73))+(1024.0/wb->getByte(74)))/2.0f;
        mRaw->metadata.wbCoeffs[2] = (float) (1024.0 /wb->getByte(75));
      } else if (wb->type == CIFF_BYTE && wb->count > 768) { // Other G series and S series cameras
        // correct offset for most cameras
        int offset = hints.get("wb_offset", 120);

        ushort16 key[] = { 0x410, 0x45f3 };
        if (! hints.has("wb_mangle"))
          key[0] = key[1] = 0;

        offset /= 2;
        mRaw->metadata.wbCoeffs[0] = (float) (wb->getU16(offset+1) ^ key[1]);
        mRaw->metadata.wbCoeffs[1] = (float) (wb->getU16(offset+0) ^ key[0]);
        mRaw->metadata.wbCoeffs[2] = (float) (wb->getU16(offset+2) ^ key[0]);
      }
    }
    if(mRootIFD->hasEntryRecursive((CiffTag)0x102c)) {
      CiffEntry *entry = mRootIFD->getEntryRecursive((CiffTag)0x102c);
      if (entry->type == CIFF_SHORT && entry->getU16() > 512) {
        // G1/Pro90 CYGM pattern
        mRaw->metadata.wbCoeffs[0] = (float) entry->getU16(62);
        mRaw->metadata.wbCoeffs[1] = (float) entry->getU16(63);
        mRaw->metadata.wbCoeffs[2] = (float) entry->getU16(60);
        mRaw->metadata.wbCoeffs[3] = (float) entry->getU16(61);
      } else if (entry->type == CIFF_SHORT) {
        /* G2, S30, S40 */
        mRaw->metadata.wbCoeffs[0] = (float) entry->getU16(51);
        mRaw->metadata.wbCoeffs[1] = ((float) entry->getU16(50) + (float) entry->getU16(53))/ 2.0f;
        mRaw->metadata.wbCoeffs[2] = (float) entry->getU16(52);
      }
    }
    if (mRootIFD->hasEntryRecursive(CIFF_SHOTINFO) && mRootIFD->hasEntryRecursive(CIFF_WHITEBALANCE)) {
      CiffEntry *shot_info = mRootIFD->getEntryRecursive(CIFF_SHOTINFO);
      ushort16 wb_index = shot_info->getU16(7);
      CiffEntry *wb_data = mRootIFD->getEntryRecursive(CIFF_WHITEBALANCE);
      /* CANON EOS D60, CANON EOS 10D, CANON EOS 300D */
      if (wb_index > 9)
        ThrowIOE("CrwDecoder: invalid white balance index");
      int wb_offset = 1 + ("0134567028"[wb_index]-'0') * 4;
      mRaw->metadata.wbCoeffs[0] = wb_data->getU16(wb_offset + 0);
      mRaw->metadata.wbCoeffs[1] = wb_data->getU16(wb_offset + 1);
      mRaw->metadata.wbCoeffs[2] = wb_data->getU16(wb_offset + 3);
    }
  } catch (const std::exception& e) {
    fprintf(stderr, "Got exception: %s\n", e.what());
    mRaw->setError(e.what());
    // We caught an exception reading WB, just ignore it
  }

  setMetaData(meta, make, model, mode, iso);
}

// The rest of this file was ported as is from dcraw.c. I don't claim to
// understand it but have tried my best to make it work safely

/*
   Construct a decode tree according the specification in *source.
   The first 16 bytes specify how many codes should be 1-bit, 2-bit
   3-bit, etc.  Bytes after that are the leaf values.

   For example, if the source is

    { 0,1,4,2,3,1,2,0,0,0,0,0,0,0,0,0,
      0x04,0x03,0x05,0x06,0x02,0x07,0x01,0x08,0x09,0x00,0x0a,0x0b,0xff  },

   then the code is

        00                0x04
        010               0x03
        011               0x05
        100               0x06
        101               0x02
        1100              0x07
        1101              0x01
        11100             0x08
        11101             0x09
        11110             0x00
        111110            0x0a
        1111110           0x0b
        1111111           0xff
 */

void CrwDecoder::makeDecoder (int n, const uchar8 *source)
{
  int max, len, h, i, j;
  const uchar8 *count;

  if (n > 1) {
    ThrowRDE("CRW: Invalid table number specified");
  }

  count = (source += 16) - 17;
  for (max=16; max && !count[max]; max--);

  if (mHuff[n] != nullptr) {
    alignedFree(mHuff[n]);
    mHuff[n] = nullptr;
  }

  auto* huff = (ushort16*)alignedMallocArray<16, ushort16, true>(1UL + (1UL << max));

  if (!huff)
    ThrowRDE("CRW: Couldn't allocate table");

  huff[0] = max;
  for (h = len = 1; len <= max; len++) {
    for (i = 0; i < count[len]; i++, ++source) {
      for (j=0; j < 1 << (max-len); j++)
        if (h <= 1 << max)
          huff[h++] = len << 8 | *source;
    }
  }

  mHuff[n] = huff;
}

void CrwDecoder::initHuffTables (uint32 table)
{
  static const uchar8 first_tree[3][29] = {
    { 0,1,4,2,3,1,2,0,0,0,0,0,0,0,0,0,
      0x04,0x03,0x05,0x06,0x02,0x07,0x01,0x08,0x09,0x00,0x0a,0x0b,0xff  },
    { 0,2,2,3,1,1,1,1,2,0,0,0,0,0,0,0,
      0x03,0x02,0x04,0x01,0x05,0x00,0x06,0x07,0x09,0x08,0x0a,0x0b,0xff  },
    { 0,0,6,3,1,1,2,0,0,0,0,0,0,0,0,0,
      0x06,0x05,0x07,0x04,0x08,0x03,0x09,0x02,0x00,0x0a,0x01,0x0b,0xff  },
  };
  static const uchar8 second_tree[3][180] = {
    { 0,2,2,2,1,4,2,1,2,5,1,1,0,0,0,139,
      0x03,0x04,0x02,0x05,0x01,0x06,0x07,0x08,
      0x12,0x13,0x11,0x14,0x09,0x15,0x22,0x00,0x21,0x16,0x0a,0xf0,
      0x23,0x17,0x24,0x31,0x32,0x18,0x19,0x33,0x25,0x41,0x34,0x42,
      0x35,0x51,0x36,0x37,0x38,0x29,0x79,0x26,0x1a,0x39,0x56,0x57,
      0x28,0x27,0x52,0x55,0x58,0x43,0x76,0x59,0x77,0x54,0x61,0xf9,
      0x71,0x78,0x75,0x96,0x97,0x49,0xb7,0x53,0xd7,0x74,0xb6,0x98,
      0x47,0x48,0x95,0x69,0x99,0x91,0xfa,0xb8,0x68,0xb5,0xb9,0xd6,
      0xf7,0xd8,0x67,0x46,0x45,0x94,0x89,0xf8,0x81,0xd5,0xf6,0xb4,
      0x88,0xb1,0x2a,0x44,0x72,0xd9,0x87,0x66,0xd4,0xf5,0x3a,0xa7,
      0x73,0xa9,0xa8,0x86,0x62,0xc7,0x65,0xc8,0xc9,0xa1,0xf4,0xd1,
      0xe9,0x5a,0x92,0x85,0xa6,0xe7,0x93,0xe8,0xc1,0xc6,0x7a,0x64,
      0xe1,0x4a,0x6a,0xe6,0xb3,0xf1,0xd3,0xa5,0x8a,0xb2,0x9a,0xba,
      0x84,0xa4,0x63,0xe5,0xc5,0xf3,0xd2,0xc4,0x82,0xaa,0xda,0xe4,
      0xf2,0xca,0x83,0xa3,0xa2,0xc3,0xea,0xc2,0xe2,0xe3,0xff,0xff  },
    { 0,2,2,1,4,1,4,1,3,3,1,0,0,0,0,140,
      0x02,0x03,0x01,0x04,0x05,0x12,0x11,0x06,
      0x13,0x07,0x08,0x14,0x22,0x09,0x21,0x00,0x23,0x15,0x31,0x32,
      0x0a,0x16,0xf0,0x24,0x33,0x41,0x42,0x19,0x17,0x25,0x18,0x51,
      0x34,0x43,0x52,0x29,0x35,0x61,0x39,0x71,0x62,0x36,0x53,0x26,
      0x38,0x1a,0x37,0x81,0x27,0x91,0x79,0x55,0x45,0x28,0x72,0x59,
      0xa1,0xb1,0x44,0x69,0x54,0x58,0xd1,0xfa,0x57,0xe1,0xf1,0xb9,
      0x49,0x47,0x63,0x6a,0xf9,0x56,0x46,0xa8,0x2a,0x4a,0x78,0x99,
      0x3a,0x75,0x74,0x86,0x65,0xc1,0x76,0xb6,0x96,0xd6,0x89,0x85,
      0xc9,0xf5,0x95,0xb4,0xc7,0xf7,0x8a,0x97,0xb8,0x73,0xb7,0xd8,
      0xd9,0x87,0xa7,0x7a,0x48,0x82,0x84,0xea,0xf4,0xa6,0xc5,0x5a,
      0x94,0xa4,0xc6,0x92,0xc3,0x68,0xb5,0xc8,0xe4,0xe5,0xe6,0xe9,
      0xa2,0xa3,0xe3,0xc2,0x66,0x67,0x93,0xaa,0xd4,0xd5,0xe7,0xf8,
      0x88,0x9a,0xd7,0x77,0xc4,0x64,0xe2,0x98,0xa5,0xca,0xda,0xe8,
      0xf3,0xf6,0xa9,0xb2,0xb3,0xf2,0xd2,0x83,0xba,0xd3,0xff,0xff  },
    { 0,0,6,2,1,3,3,2,5,1,2,2,8,10,0,117,
      0x04,0x05,0x03,0x06,0x02,0x07,0x01,0x08,
      0x09,0x12,0x13,0x14,0x11,0x15,0x0a,0x16,0x17,0xf0,0x00,0x22,
      0x21,0x18,0x23,0x19,0x24,0x32,0x31,0x25,0x33,0x38,0x37,0x34,
      0x35,0x36,0x39,0x79,0x57,0x58,0x59,0x28,0x56,0x78,0x27,0x41,
      0x29,0x77,0x26,0x42,0x76,0x99,0x1a,0x55,0x98,0x97,0xf9,0x48,
      0x54,0x96,0x89,0x47,0xb7,0x49,0xfa,0x75,0x68,0xb6,0x67,0x69,
      0xb9,0xb8,0xd8,0x52,0xd7,0x88,0xb5,0x74,0x51,0x46,0xd9,0xf8,
      0x3a,0xd6,0x87,0x45,0x7a,0x95,0xd5,0xf6,0x86,0xb4,0xa9,0x94,
      0x53,0x2a,0xa8,0x43,0xf5,0xf7,0xd4,0x66,0xa7,0x5a,0x44,0x8a,
      0xc9,0xe8,0xc8,0xe7,0x9a,0x6a,0x73,0x4a,0x61,0xc7,0xf4,0xc6,
      0x65,0xe9,0x72,0xe6,0x71,0x91,0x93,0xa6,0xda,0x92,0x85,0x62,
      0xf3,0xc5,0xb2,0xa4,0x84,0xba,0x64,0xa5,0xb3,0xd2,0x81,0xe5,
      0xd3,0xaa,0xc4,0xca,0xf2,0xb1,0xe4,0xd1,0x83,0x63,0xea,0xc3,
      0xe2,0x82,0xf1,0xa3,0xc2,0xa1,0xc1,0xe3,0xa2,0xe1,0xff,0xff  }
  };
  makeDecoder(0, first_tree[table]);
  makeDecoder(1, second_tree[table]);
}

uint32 CrwDecoder::getbithuff (BitPumpJPEG &pump, int nbits, ushort16 *huff)
{
  uint32 c = pump.peekBits(nbits);
  // Skip bits given by the high order bits of the huff table
  pump.getBitsSafe(huff[c] >> 8);
  // Return the lower order bits
  return (uchar8) huff[c];
}

void CrwDecoder::decodeRaw(bool lowbits, uint32 dec_table, uint32 width, uint32 height)
{
  int nblocks;
  int block, diffbuf[64], leaf, len, diff, carry=0, pnum=0, base[2];

  initHuffTables (dec_table);

  uint32 offset = 540 + lowbits*height*width/4;
  ByteStream input(mFile, offset);
  BitPumpJPEG pump(input);

  for (uint32 row=0; row < height; row+=8) {
    auto *dest = (ushort16 *)&mRaw->getData()[row * width * 2];
    nblocks = min(8u, height - row) * width >> 6;
    for (block=0; block < nblocks; block++) {
      memset (diffbuf, 0, sizeof diffbuf);
      for (uint32 i=0; i < 64; i++ ) {
        leaf = getbithuff(pump, *mHuff[i > 0], mHuff[i > 0]+1);
        if (leaf == 0 && i) break;
        if (leaf == 0xff) continue;
        i  += leaf >> 4;
        len = leaf & 15;
        if (len == 0) continue;
        diff = pump.getBitsSafe(len);
        if ((diff & (1 << (len-1))) == 0)
          diff -= (1 << len) - 1;
        if (i < 64) diffbuf[i] = diff;
      }
      diffbuf[0] += carry;
      carry = diffbuf[0];
      for (uint32 i=0; i < 64; i++ ) {
        if (pnum++ % width == 0)
          base[0] = base[1] = 512;
        if ((dest[(block << 6) + i] = base[i & 1] += diffbuf[i]) >> 10)
          ThrowRDE("CRW: Error decompressing");
      }
    }

    // Add the uncompressed 2 low bits to the decoded 8 high bits
    if (lowbits) {
      offset = 26 + row*width/4;
      ByteStream lowbit_input(mFile, offset, height*width/4);
      uint32 lines =
          min(height - row, 8u); // Process 8 rows or however are left
      for (uint32 i=0; i < width/4*lines; i++) {
        uint32 c = ((uint32) lowbit_input.getByte());
        for (uint32 r=0; r < 8; r+=2, dest++) { // Process 8 bits in pairs
          ushort16 val = (*dest << 2) | ((c >> r) & 0x0003);
          if (width == 2672 && val < 512) val += 2; // No idea why this is needed
          *dest = val;
        }
      }
    }
  }
}

} // namespace RawSpeed
