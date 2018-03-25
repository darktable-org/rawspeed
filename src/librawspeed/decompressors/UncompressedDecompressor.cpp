/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
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

#include "decompressors/UncompressedDecompressor.h"
#include "common/Common.h"                // for uint32, uchar8, ushort16
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpLSB.h"                // for BitPumpLSB
#include "io/BitPumpMSB.h"                // for BitPumpMSB
#include "io/BitPumpMSB16.h"              // for BitPumpMSB16
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32
#include "io/ByteStream.h"                // for ByteStream
#include "io/Endianness.h"                // for getHostEndianness, Endiann...
#include "io/IOException.h"               // for ThrowIOE
#include <algorithm>                      // for min
#include <cassert>                        // for assert

using std::min;

namespace rawspeed {

void UncompressedDecompressor::sanityCheck(const uint32* h, int bpl) {
  assert(h != nullptr);
  assert(*h > 0);
  assert(bpl > 0);
  assert(input.getSize() > 0);

  // How many multiples of bpl are there in the input buffer?
  // The remainder is ignored/discarded.
  const auto fullRows = input.getRemainSize() / bpl;

  // If more than the output height, we are all good.
  if (fullRows >= *h)
    return; // all good!

  if (fullRows == 0)
    ThrowIOE("Not enough data to decode a single line. Image file truncated.");

  ThrowIOE("Image truncated, only %u of %u lines found", fullRows, *h);

  // FIXME: need to come up with some common variable to allow proceeding here
  // *h = min_h;
}

void UncompressedDecompressor::sanityCheck(uint32 w, const uint32* h, int bpp) {
  assert(w > 0);
  assert(bpp > 0);

  // bytes per line
  const auto bpl = bpp * w;
  assert(bpl > 0);

  sanityCheck(h, bpl);
}

int UncompressedDecompressor::bytesPerLine(int w, bool skips) {
  assert(w > 0);

  if ((12 * w) % 8 != 0)
    ThrowIOE("Bad image width");

  // Calculate expected bytes per line.
  auto perline = (12 * w) / 8;

  if (!skips)
    return perline;

  // Add skips every 10 pixels
  perline += ((w + 2) / 10);

  return perline;
}

void UncompressedDecompressor::readUncompressedRaw(const iPoint2D& size,
                                                   const iPoint2D& offset,
                                                   int inputPitch,
                                                   int bitPerPixel,
                                                   BitOrder order) {
  assert(inputPitch > 0);
  assert(bitPerPixel > 0);

  uchar8* data = mRaw->getData();
  uint32 outPitch = mRaw->pitch;
  uint32 w = size.x;
  uint32 h = size.y;
  uint32 cpp = mRaw->getCpp();
  uint64 ox = offset.x;
  uint64 oy = offset.y;

  sanityCheck(&h, inputPitch);

  if (bitPerPixel > 16 && mRaw->getDataType() == TYPE_USHORT16)
    ThrowRDE("Unsupported bit depth");

  const int outPixelBits = w * cpp * bitPerPixel;
  assert(outPixelBits > 0);

  if (outPixelBits % 8 != 0) {
    ThrowRDE("Bad combination of cpp (%u), bps (%u) and width (%u), the "
             "pitch is %u bits, which is not a multiple of 8 (1 byte)",
             cpp, bitPerPixel, w, outPixelBits);
  }

  const int outPixelBytes = outPixelBits / 8;

  uint32 skipBits = inputPitch - outPixelBytes; // Skip per line
  if (oy > static_cast<uint64>(mRaw->dim.y))
    ThrowRDE("Invalid y offset");
  if (ox + size.x > static_cast<uint64>(mRaw->dim.x))
    ThrowRDE("Invalid x offset");

  uint64 y = oy;
  h = min(h + oy, static_cast<uint64>(mRaw->dim.y));

  if (mRaw->getDataType() == TYPE_FLOAT32) {
    if (bitPerPixel != 32)
      ThrowRDE("Only 32 bit float point supported");
    copyPixels(&data[offset.x * sizeof(float) * cpp + y * outPitch], outPitch,
               input.getData(inputPitch * (h - y)), inputPitch,
               w * mRaw->getBpp(), h - y);
    return;
  }

  if (BitOrder_MSB == order) {
    BitPumpMSB bits(input);
    w *= cpp;
    for (; y < h; y++) {
      auto* dest = reinterpret_cast<ushort16*>(
          &data[offset.x * sizeof(ushort16) * cpp + y * outPitch]);
      for (uint32 x = 0; x < w; x++) {
        uint32 b = bits.getBits(bitPerPixel);
        dest[x] = b;
      }
      bits.skipBits(skipBits);
    }
  } else if (BitOrder_MSB16 == order) {
    BitPumpMSB16 bits(input);
    w *= cpp;
    for (; y < h; y++) {
      auto* dest = reinterpret_cast<ushort16*>(
          &data[offset.x * sizeof(ushort16) * cpp + y * outPitch]);
      for (uint32 x = 0; x < w; x++) {
        uint32 b = bits.getBits(bitPerPixel);
        dest[x] = b;
      }
      bits.skipBits(skipBits);
    }
  } else if (BitOrder_MSB32 == order) {
    BitPumpMSB32 bits(input);
    w *= cpp;
    for (; y < h; y++) {
      auto* dest = reinterpret_cast<ushort16*>(
          &data[offset.x * sizeof(ushort16) * cpp + y * outPitch]);
      for (uint32 x = 0; x < w; x++) {
        uint32 b = bits.getBits(bitPerPixel);
        dest[x] = b;
      }
      bits.skipBits(skipBits);
    }
  } else {
    if (bitPerPixel == 16 && getHostEndianness() == Endianness::little) {
      copyPixels(&data[offset.x * sizeof(ushort16) * cpp + y * outPitch],
                 outPitch, input.getData(inputPitch * (h - y)), inputPitch,
                 w * mRaw->getBpp(), h - y);
      return;
    }
    if (bitPerPixel == 12 && static_cast<int>(w) == inputPitch * 8 / 12 &&
        getHostEndianness() == Endianness::little) {
      decode12BitRaw<Endianness::little>(w, h);
      return;
    }
    BitPumpLSB bits(input);
    w *= cpp;
    for (; y < h; y++) {
      auto* dest = reinterpret_cast<ushort16*>(
          &data[offset.x * sizeof(ushort16) + y * outPitch]);
      for (uint32 x = 0; x < w; x++) {
        uint32 b = bits.getBits(bitPerPixel);
        dest[x] = b;
      }
      bits.skipBits(skipBits);
    }
  }
}

template <bool uncorrectedRawValues>
void UncompressedDecompressor::decode8BitRaw(uint32 w, uint32 h) {
  sanityCheck(w, &h, 1);

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(w * h);
  uint32 random = 0;
  for (uint32 y = 0; y < h; y++) {
    auto* dest = reinterpret_cast<ushort16*>(&data[y * pitch]);
    for (uint32 x = 0; x < w; x++) {
      if (uncorrectedRawValues)
        dest[x] = *in;
      else
        mRaw->setWithLookUp(*in, reinterpret_cast<uchar8*>(&dest[x]), &random);
      in++;
    }
  }
}

template void UncompressedDecompressor::decode8BitRaw<false>(uint32 w, uint32 h);
template void UncompressedDecompressor::decode8BitRaw<true>(uint32 w, uint32 h);

template <Endianness e, bool interlaced, bool skips>
void UncompressedDecompressor::decode12BitRaw(uint32 w, uint32 h) {
  static constexpr const auto bits = 12;

  static_assert(e == Endianness::little || e == Endianness::big,
                "unknown endiannes");

  static constexpr const auto shift = 16 - bits;
  static constexpr const auto pack = 8 - shift;
  static constexpr const auto mask = (1 << pack) - 1;

  static_assert(bits == 12 && pack == 4, "wrong pack");

  static_assert(bits == 12 && mask == 0x0f, "wrong mask");

  uint32 perline = bytesPerLine(w, skips);

  sanityCheck(&h, perline);

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;

  // FIXME: maybe check size of interlaced data?
  const uchar8* in = input.peekData(perline * h);
  uint32 half = (h + 1) >> 1;
  for (uint32 row = 0; row < h; row++) {
    uint32 y = !interlaced ? row : row % half * 2 + row / half;
    auto* dest = reinterpret_cast<ushort16*>(&data[y * pitch]);

    if (interlaced && y == 1) {
      // The second field starts at a 2048 byte aligment
      const uint32 offset = ((half * w * 3 / 2 >> 11) + 1) << 11;
      input.skipBytes(offset);
      in = input.peekData(perline * (h - row));
    }

    for (uint32 x = 0; x < w; x += 2, in += 3) {
      uint32 g1 = in[0];
      uint32 g2 = in[1];

      auto process = [dest](uint32 i, bool invert, uint32 p1, uint32 p2) {
        if (!(invert ^ (e == Endianness::little)))
          dest[i] = (p1 << pack) | (p2 >> pack);
        else
          dest[i] = ((p2 & mask) << 8) | p1;
      };

      process(x, false, g1, g2);

      g1 = in[2];

      process(x + 1, true, g1, g2);

      if (skips && ((x % 10) == 8))
        in++;
    }
  }
  input.skipBytes(input.getRemainSize());
}

template void
UncompressedDecompressor::decode12BitRaw<Endianness::little, false, false>(
    uint32 w, uint32 h);
template void
UncompressedDecompressor::decode12BitRaw<Endianness::big, false, false>(
    uint32 w, uint32 h);
template void
UncompressedDecompressor::decode12BitRaw<Endianness::big, true, false>(
    uint32 w, uint32 h);
template void
UncompressedDecompressor::decode12BitRaw<Endianness::little, false, true>(
    uint32 w, uint32 h);
template void
UncompressedDecompressor::decode12BitRaw<Endianness::big, false, true>(
    uint32 w, uint32 h);

template <Endianness e>
void UncompressedDecompressor::decode12BitRawUnpackedLeftAligned(uint32 w,
                                                                 uint32 h) {
  static_assert(e == Endianness::big, "unknown endiannes");

  sanityCheck(w, &h, 2);

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(w * h * 2);

  for (uint32 y = 0; y < h; y++) {
    auto* dest = reinterpret_cast<ushort16*>(&data[y * pitch]);
    for (uint32 x = 0; x < w; x += 1, in += 2) {
      uint32 g1 = in[0];
      uint32 g2 = in[1];

      if (e == Endianness::big)
        dest[x] = (((g1 << 8) | (g2 & 0xf0)) >> 4);
    }
  }
}

template void
UncompressedDecompressor::decode12BitRawUnpackedLeftAligned<Endianness::big>(
    uint32 w, uint32 h);

template <int bits, Endianness e>
void UncompressedDecompressor::decodeRawUnpacked(uint32 w, uint32 h) {
  static_assert(bits == 12 || bits == 14 || bits == 16, "unhandled bitdepth");
  static_assert(e == Endianness::little || e == Endianness::big,
                "unknown endiannes");

  static constexpr const auto shift = 16 - bits;
  static constexpr const auto mask = (1 << (8 - shift)) - 1;

  static_assert((bits == 12 && mask == 0x0f) || bits != 12, "wrong mask");
  static_assert((bits == 14 && mask == 0x3f) || bits != 14, "wrong mask");
  static_assert((bits == 16 && mask == 0xff) || bits != 16, "wrong mask");

  sanityCheck(w, &h, 2);

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(w * h * 2);

  for (uint32 y = 0; y < h; y++) {
    auto* dest = reinterpret_cast<ushort16*>(&data[y * pitch]);
    for (uint32 x = 0; x < w; x += 1, in += 2) {
      uint32 g1 = in[0];
      uint32 g2 = in[1];

      if (e == Endianness::little)
        dest[x] = ((g2 << 8) | g1) >> shift;
      else
        dest[x] = ((g1 & mask) << 8) | g2;
    }
  }
}

template void
UncompressedDecompressor::decodeRawUnpacked<12, Endianness::little>(uint32 w,
                                                                    uint32 h);
template void
UncompressedDecompressor::decodeRawUnpacked<12, Endianness::big>(uint32 w,
                                                                 uint32 h);
template void
UncompressedDecompressor::decodeRawUnpacked<14, Endianness::big>(uint32 w,
                                                                 uint32 h);
template void
UncompressedDecompressor::decodeRawUnpacked<16, Endianness::little>(uint32 w,
                                                                    uint32 h);
template void
UncompressedDecompressor::decodeRawUnpacked<16, Endianness::big>(uint32 w,
                                                                 uint32 h);

} // namespace rawspeed
