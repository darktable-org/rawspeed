/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "decompressors/NikonDecompressor.h"
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Common.h"                // for extractHighBits, clampBits
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/BitPumpMSB.h"                // for BitPumpMSB, BitStream<>::f...
#include "io/Buffer.h"                    // for Buffer
#include "io/ByteStream.h"                // for ByteStream
#include <cassert>                        // for assert
#include <cstdint>                        // for uint32_t, uint16_t, int16_t
#include <cstdio>                         // for size_t
#include <vector>                         // for vector

namespace rawspeed {

const std::array<std::array<std::array<uint8_t, 16>, 2>, 6>
    NikonDecompressor::nikon_tree = {{
        {{/* 12-bit lossy */
          {0, 1, 5, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
          {5, 4, 3, 6, 2, 7, 1, 0, 8, 9, 11, 10, 12}}},
        {{/* 12-bit lossy after split */
          {0, 1, 5, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
          {0x39, 0x5a, 0x38, 0x27, 0x16, 5, 4, 3, 2, 1, 0, 11, 12, 12}}},
        {{/* 12-bit lossless */
          {0, 1, 4, 2, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0},
          {5, 4, 6, 3, 7, 2, 8, 1, 9, 0, 10, 11, 12}}},
        {{/* 14-bit lossy */
          {0, 1, 4, 3, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
          {5, 6, 4, 7, 8, 3, 9, 2, 1, 0, 10, 11, 12, 13, 14}}},
        {{/* 14-bit lossy after split */
          {0, 1, 5, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0},
          {8, 0x5c, 0x4b, 0x3a, 0x29, 7, 6, 5, 4, 3, 2, 1, 0, 13, 14}}},
        {{/* 14-bit lossless */
          {0, 1, 4, 2, 2, 3, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0},
          {7, 6, 8, 5, 9, 4, 10, 3, 11, 12, 2, 0, 1, 13, 14}}},
    }};

namespace {

const std::array<uint32_t, 32> bitMask = {
    {0xffffffff, 0x7fffffff, 0x3fffffff, 0x1fffffff, 0x0fffffff, 0x07ffffff,
     0x03ffffff, 0x01ffffff, 0x00ffffff, 0x007fffff, 0x003fffff, 0x001fffff,
     0x000fffff, 0x0007ffff, 0x0003ffff, 0x0001ffff, 0x0000ffff, 0x00007fff,
     0x00003fff, 0x00001fff, 0x00000fff, 0x000007ff, 0x000003ff, 0x000001ff,
     0x000000ff, 0x0000007f, 0x0000003f, 0x0000001f, 0x0000000f, 0x00000007,
     0x00000003, 0x00000001}};

class NikonLASDecompressor {
  bool mUseBigtable = true;
  bool mDNGCompatible = false;

  struct HuffmanTable {
    /*
     * These two fields directly represent the contents of a JPEG DHT
     * marker
     */
    std::array<uint32_t, 17> bits;
    std::array<uint32_t, 256> huffval;

    /*
     * The remaining fields are computed from the above to allow more
     * efficient coding and decoding.  These fields should be considered
     * private to the Huffman compression & decompression modules.
     */

    std::array<uint16_t, 17> mincode;
    std::array<int, 18> maxcode;
    std::array<int16_t, 17> valptr;
    std::array<uint32_t, 256> numbits;
    std::vector<int> bigTable;
    bool initialized;
  } dctbl1;

  void createHuffmanTable() {
    int p;
    int i;
    int l;
    int lastp;
    int si;
    std::array<char, 257> huffsize;
    std::array<uint16_t, 257> huffcode;
    uint16_t code;
    int size;
    int value;
    int ll;
    int ul;

    /*
     * Figure C.1: make table of Huffman code length for each symbol
     * Note that this is in code-length order.
     */
    p = 0;
    for (l = 1; l <= 16; l++) {
      for (i = 1; i <= static_cast<int>(dctbl1.bits[l]); i++) {
        huffsize[p++] = static_cast<char>(l);
        if (p > 256)
          ThrowRDE("LJpegDecompressor::createHuffmanTable: Code length too "
                   "long. Corrupt data.");
      }
    }
    huffsize[p] = 0;
    lastp = p;

    /*
     * Figure C.2: generate the codes themselves
     * Note that this is in code-length order.
     */
    code = 0;
    si = huffsize[0];
    p = 0;
    while (huffsize[p]) {
      while ((static_cast<int>(huffsize[p])) == si) {
        huffcode[p++] = code;
        code++;
      }
      code <<= 1;
      si++;
      if (p > 256)
        ThrowRDE("createHuffmanTable: Code length too long. Corrupt data.");
    }

    /*
     * Figure F.15: generate decoding tables
     */
    dctbl1.mincode[0] = 0;
    dctbl1.maxcode[0] = 0;
    p = 0;
    for (l = 1; l <= 16; l++) {
      if (dctbl1.bits[l]) {
        dctbl1.valptr[l] = p;
        dctbl1.mincode[l] = huffcode[p];
        p += dctbl1.bits[l];
        dctbl1.maxcode[l] = huffcode[p - 1];
      } else {
        dctbl1.valptr[l] =
            0xff; // This check must be present to avoid crash on junk
        dctbl1.maxcode[l] = -1;
      }
      if (p > 256)
        ThrowRDE("createHuffmanTable: Code length too long. Corrupt data.");
    }

    /*
     * We put in this value to ensure HuffDecode terminates.
     */
    dctbl1.maxcode[17] = 0xFFFFFL;

    /*
     * Build the numbits, value lookup tables.
     * These table allow us to gather 8 bits from the bits stream,
     * and immediately lookup the size and value of the huffman codes.
     * If size is zero, it means that more than 8 bits are in the huffman
     * code (this happens about 3-4% of the time).
     */
    dctbl1.numbits.fill(0);
    for (p = 0; p < lastp; p++) {
      size = huffsize[p];
      if (size <= 8) {
        value = dctbl1.huffval[p];
        code = huffcode[p];
        ll = code << (8 - size);
        if (size < 8) {
          ul = ll | bitMask[24 + size];
        } else {
          ul = ll;
        }
        if (ul > 256 || ll > ul)
          ThrowRDE("createHuffmanTable: Code length too long. Corrupt data.");
        for (i = ll; i <= ul; i++) {
          dctbl1.numbits[i] = size | (value << 4);
        }
      }
    }
    if (mUseBigtable)
      createBigTable();
    dctbl1.initialized = true;
  }

  /************************************
   * Bitable creation
   *
   * This is expanding the concept of fast lookups
   *
   * A complete table for 14 arbitrary bits will be
   * created that enables fast lookup of number of bits used,
   * and final delta result.
   * Hit rate is about 90-99% for typical LJPEGS, usually about 98%
   *
   ************************************/

  void createBigTable() {
    const uint32_t bits =
        14; // HuffDecode functions must be changed, if this is modified.
    const uint32_t size = 1 << bits;
    int rv = 0;
    int temp;
    uint32_t l;

    dctbl1.bigTable.resize(size);
    for (uint32_t i = 0; i < size; i++) {
      uint16_t input = i << 2; // Calculate input value
      int code = input >> 8;   // Get 8 bits
      uint32_t val = dctbl1.numbits[code];
      l = val & 15;
      if (l) {
        rv = val >> 4;
      } else {
        l = 8;
        while (code > dctbl1.maxcode[l]) {
          temp = extractHighBits(input, l, /*effectiveBitwidth=*/15) & 1;
          code = (code << 1) | temp;
          l++;
        }

        /*
         * With garbage input we may reach the sentinel value l = 17.
         */

        if (l > 16 || dctbl1.valptr[l] == 0xff) {
          dctbl1.bigTable[i] = 0xff;
          continue;
        }
        rv = dctbl1.huffval[dctbl1.valptr[l] + (code - dctbl1.mincode[l])];
      }

      if (rv == 16) {
        if (mDNGCompatible)
          dctbl1.bigTable[i] = (-(32768 << 8)) | (16 + l);
        else
          dctbl1.bigTable[i] = (-(32768 << 8)) | l;
        continue;
      }

      if (rv + l > bits) {
        dctbl1.bigTable[i] = 0xff;
        continue;
      }

      if (rv) {
        int x = extractHighBits(input, l + rv) & ((1 << rv) - 1);
        if ((x & (1 << (rv - 1))) == 0)
          x -= (1 << rv) - 1;
        dctbl1.bigTable[i] =
            static_cast<int>((static_cast<unsigned>(x) << 8) | (l + rv));
      } else {
        dctbl1.bigTable[i] = l;
      }
    }
  }

public:
  uint32_t setNCodesPerLength(const Buffer& data) {
    uint32_t acc = 0;
    for (uint32_t i = 0; i < 16; i++) {
      dctbl1.bits[i + 1] = data[i];
      acc += dctbl1.bits[i + 1];
    }
    dctbl1.bits[0] = 0;
    return acc;
  }

  void setCodeValues(const Buffer& data) {
    for (uint32_t i = 0; i < data.getSize(); i++)
      dctbl1.huffval[i] = data[i];
  }

  void setup(bool fullDecode_, bool fixDNGBug16_) { createHuffmanTable(); }

  /*
   *--------------------------------------------------------------
   *
   * HuffDecode --
   *
   * Taken from Figure F.16: extract next coded symbol from
   * input stream.  This should becode a macro.
   *
   * Results:
   * Next coded symbol
   *
   * Side effects:
   * Bitstream is parsed.
   *
   *--------------------------------------------------------------
   */
  int decodeDifference(BitPumpMSB& bits) { // NOLINT: google-runtime-references
    int rv;
    int l;
    int temp;
    int code;
    unsigned val;

    bits.fill();
    code = bits.peekBitsNoFill(14);
    val = static_cast<unsigned>(dctbl1.bigTable[code]);
    if ((val & 0xff) != 0xff) {
      bits.skipBitsNoFill(val & 0xff);
      return static_cast<int>(val) >> 8;
    }

    rv = 0;
    code = bits.peekBitsNoFill(8);
    val = dctbl1.numbits[code];
    l = val & 15;
    if (l) {
      bits.skipBitsNoFill(l);
      rv = static_cast<int>(val) >> 4;
    } else {
      bits.skipBitsNoFill(8);
      l = 8;
      while (code > dctbl1.maxcode[l]) {
        temp = bits.getBitsNoFill(1);
        code = (code << 1) | temp;
        l++;
      }

      if (l > 16) {
        ThrowRDE("Corrupt JPEG data: bad Huffman code:%u\n", l);
      } else {
        rv = dctbl1.huffval[dctbl1.valptr[l] + (code - dctbl1.mincode[l])];
      }
    }

    if (rv == 16)
      return -32768;

    /*
     * Section F.2.2.1: decode the difference and
     * Figure F.12: extend sign bit
     */
    uint32_t len = rv & 15;
    uint32_t shl = rv >> 4;
    int diff = ((bits.getBits(len - shl) << 1) + 1) << shl >> 1;
    if ((diff & (1 << (len - 1))) == 0)
      diff -= (1 << len) - !shl;
    return diff;
  }
};

} // namespace

std::vector<uint16_t> NikonDecompressor::createCurve(ByteStream& metadata,
                                                     uint32_t bitsPS,
                                                     uint32_t v0, uint32_t v1,
                                                     uint32_t* split) {
  // Nikon Z7 12/14 bit compressed hack.
  if (v0 == 68 && v1 == 64)
    bitsPS -= 2;

  // 'curve' will hold a peace wise linearly interpolated function.
  // there are 'csize' segments, each is 'step' values long.
  // the very last value is not part of the used table but necessary
  // to linearly interpolate the last segment, therefore the '+1/-1'
  // size adjustments of 'curve'.
  std::vector<uint16_t> curve((1 << bitsPS & 0x7fff) + 1);
  assert(curve.size() > 1);

  for (size_t i = 0; i < curve.size(); i++)
    curve[i] = i;

  uint32_t step = 0;
  uint32_t csize = metadata.getU16();
  if (csize > 1)
    step = curve.size() / (csize - 1);

  if (v0 == 68 && (v1 == 32 || v1 == 64) && step > 0) {
    if ((csize - 1) * step != curve.size() - 1)
      ThrowRDE("Bad curve segment count (%u)", csize);

    for (size_t i = 0; i < csize; i++)
      curve[i * step] = metadata.getU16();
    for (size_t i = 0; i < curve.size() - 1; i++) {
      const uint32_t b_scale = i % step;

      const uint32_t a_pos = i - b_scale;
      const uint32_t b_pos = a_pos + step;
      assert(a_pos < curve.size());
      assert(b_pos > 0);
      assert(b_pos < curve.size());
      assert(a_pos < b_pos);

      const uint32_t a_scale = step - b_scale;
      curve[i] = (a_scale * curve[a_pos] + b_scale * curve[b_pos]) / step;
    }

    metadata.setPosition(562);
    *split = metadata.getU16();
  } else if (v0 != 70) {
    if (csize == 0 || csize > 0x4001)
      ThrowRDE("Don't know how to compute curve! csize = %u", csize);

    curve.resize(csize + 1UL);
    assert(curve.size() > 1);

    for (uint32_t i = 0; i < csize; i++) {
      curve[i] = metadata.getU16();
    }
  }

  // and drop the last value
  curve.resize(curve.size() - 1);
  assert(!curve.empty());

  return curve;
}

template <typename Huffman>
Huffman NikonDecompressor::createHuffmanTable(uint32_t huffSelect) {
  Huffman ht;
  uint32_t count =
      ht.setNCodesPerLength(Buffer(nikon_tree[huffSelect][0].data(), 16));
  ht.setCodeValues(Buffer(nikon_tree[huffSelect][1].data(), count));
  ht.setup(true, false);
  return ht;
}

NikonDecompressor::NikonDecompressor(const RawImage& raw, ByteStream metadata,
                                     uint32_t bitsPS_)
    : mRaw(raw), bitsPS(bitsPS_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  if (mRaw->dim.x == 0 || mRaw->dim.y == 0 || mRaw->dim.x % 2 != 0 ||
      mRaw->dim.x > 8288 || mRaw->dim.y > 5520)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", mRaw->dim.x,
             mRaw->dim.y);

  switch (bitsPS) {
  case 12:
  case 14:
    break;
  default:
    ThrowRDE("Invalid bpp found: %u", bitsPS);
  }

  uint32_t v0 = metadata.getByte();
  uint32_t v1 = metadata.getByte();

  writeLog(DEBUG_PRIO_EXTRA, "Nef version v0:%u, v1:%u", v0, v1);

  if (v0 == 73 || v1 == 88)
    metadata.skipBytes(2110);

  if (v0 == 70)
    huffSelect = 2;
  if (bitsPS == 14)
    huffSelect += 3;

  pUp[0][0] = metadata.getU16();
  pUp[1][0] = metadata.getU16();
  pUp[0][1] = metadata.getU16();
  pUp[1][1] = metadata.getU16();

  curve = createCurve(metadata, bitsPS, v0, v1, &split);

  // If the 'split' happens outside of the image, it does not actually happen.
  if (split >= static_cast<unsigned>(mRaw->dim.y))
    split = 0;
}

template <typename Huffman>
void NikonDecompressor::decompress(BitPumpMSB& bits, int start_y, int end_y) {
  auto ht = createHuffmanTable<Huffman>(huffSelect);

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  // allow gcc to devirtualize the calls below
  auto* rawdata = reinterpret_cast<RawImageDataU16*>(mRaw.get());

  assert(out.width % 2 == 0);
  assert(out.width >= 2);
  for (int row = start_y; row < end_y; row++) {
    std::array<int, 2> pred = pUp[row & 1];
    for (int col = 0; col < out.width; col++) {
      pred[col & 1] += ht.decodeDifference(bits);
      if (col < 2)
        pUp[row & 1][col & 1] = pred[col & 1];
      rawdata->setWithLookUp(clampBits(pred[col & 1], 15),
                             reinterpret_cast<uint8_t*>(&out(row, col)),
                             &random);
    }
  }
}

void NikonDecompressor::decompress(const ByteStream& data,
                                   bool uncorrectedRawValues) {
  RawImageCurveGuard curveHandler(&mRaw, curve, uncorrectedRawValues);

  BitPumpMSB bits(data);

  random = bits.peekBits(24);

  assert(split == 0 || split < static_cast<unsigned>(mRaw->dim.y));

  if (!split) {
    decompress<HuffmanTable>(bits, 0, mRaw->dim.y);
  } else {
    decompress<HuffmanTable>(bits, 0, split);
    huffSelect += 1;
    decompress<NikonLASDecompressor>(bits, split, mRaw->dim.y);
  }
}

} // namespace rawspeed
