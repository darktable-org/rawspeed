/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2017-2019 Roman Lebedev

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
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Common.h"                // for BitOrder, copyPixels, BitO...
#include "common/FloatingPoint.h"         // for Binary16, Binary24, Binary...
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpLSB.h"                // for BitPumpLSB, BitStream<>::f...
#include "io/BitPumpMSB.h"                // for BitPumpMSB, BitStream<>::f...
#include "io/BitPumpMSB16.h"              // for BitPumpMSB16, BitStream<>:...
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32, BitStream<>:...
#include "io/ByteStream.h"                // for ByteStream
#include "io/Endianness.h"                // for Endianness, Endianness::li...
#include "io/IOException.h"               // for ThrowException, ThrowIOE
#include <algorithm>                      // for min
#include <cassert>                        // for assert

using std::min;

namespace rawspeed {

void UncompressedDecompressor::sanityCheck(const uint32_t* h,
                                           int bytesPerLine) const {
  assert(h != nullptr);
  assert(*h > 0);
  assert(bytesPerLine > 0);
  assert(input.getSize() > 0);

  // How many multiples of bpl are there in the input buffer?
  // The remainder is ignored/discarded.
  const auto fullRows = input.getRemainSize() / bytesPerLine;

  // If more than the output height, we are all good.
  if (fullRows >= *h)
    return; // all good!

  if (fullRows == 0)
    ThrowIOE("Not enough data to decode a single line. Image file truncated.");

  ThrowIOE("Image truncated, only %u of %u lines found", fullRows, *h);

  // FIXME: need to come up with some common variable to allow proceeding here
  // *h = min_h;
}

void UncompressedDecompressor::sanityCheck(uint32_t w, const uint32_t* h,
                                           int bpp) const {
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

template <typename Pump, typename NarrowFpType>
void UncompressedDecompressor::decodePackedFP(const iPoint2D& size,
                                              const iPoint2D& offset,
                                              uint32_t skipBytes, int rows,
                                              int row) const {
  const Array2DRef<float> out(mRaw->getF32DataAsUncroppedArray2DRef());
  Pump bits(input);

  int cols = size.x * mRaw->getCpp();
  for (; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      uint32_t b = bits.getBits(NarrowFpType::StorageWidth);
      uint32_t f =
          extendBinaryFloatingPoint<NarrowFpType, ieee_754_2008::Binary32>(b);
      out(row, offset.x + col) = bit_cast<float>(f);
    }
    bits.skipBytes(skipBytes);
  }
}

template <typename Pump>
void UncompressedDecompressor::decodePackedInt(const iPoint2D& size,
                                               const iPoint2D& offset,
                                               uint32_t skipBytes, int rows,
                                               int row, int bitPerPixel) const {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  Pump bits(input);

  int cols = size.x * mRaw->getCpp();
  for (; row < rows; row++) {
    for (int x = 0; x < cols; x++) {
      out(row, x) = bits.getBits(bitPerPixel);
    }
    bits.skipBytes(skipBytes);
  }
}

void UncompressedDecompressor::readUncompressedRaw(const iPoint2D& size,
                                                   const iPoint2D& offset,
                                                   int inputPitchBytes,
                                                   int bitPerPixel,
                                                   BitOrder order) {
  assert(inputPitchBytes > 0);
  assert(bitPerPixel > 0);

  uint32_t outPitch = mRaw->pitch;
  uint32_t w = size.x;
  uint32_t h = size.y;
  uint32_t cpp = mRaw->getCpp();
  uint64_t ox = offset.x;
  uint64_t oy = offset.y;

  if (bitPerPixel > 16 && mRaw->getDataType() == RawImageType::UINT16)
    ThrowRDE("Unsupported bit depth");

  const int outPixelBits = w * cpp * bitPerPixel;
  assert(outPixelBits > 0);

  if (outPixelBits % 8 != 0) {
    ThrowRDE("Bad combination of cpp (%u), bps (%u) and width (%u), the "
             "pitch is %u bits, which is not a multiple of 8 (1 byte)",
             cpp, bitPerPixel, w, outPixelBits);
  }

  const int outPixelBytes = outPixelBits / 8;

  // The input pitch might be larger than needed (i.e. have some padding),
  // but it can *not* be smaller than needed.
  if (inputPitchBytes < outPixelBytes)
    ThrowRDE("Specified pitch is smaller than minimally-required pitch");

  // Check the specified pitch, not the minimally-required pitch.
  sanityCheck(&h, inputPitchBytes);

  assert(inputPitchBytes >= outPixelBytes);
  uint32_t skipBytes = inputPitchBytes - outPixelBytes; // Skip per line

  if (oy > static_cast<uint64_t>(mRaw->dim.y))
    ThrowRDE("Invalid y offset");
  if (ox + size.x > static_cast<uint64_t>(mRaw->dim.x))
    ThrowRDE("Invalid x offset");

  uint64_t y = oy;
  h = min(h + oy, static_cast<uint64_t>(mRaw->dim.y));

  if (mRaw->getDataType() == RawImageType::F32) {
    if (bitPerPixel == 32) {
      const Array2DRef<float> out(mRaw->getF32DataAsUncroppedArray2DRef());
      copyPixels(reinterpret_cast<uint8_t*>(&out(y, offset.x * cpp)), outPitch,
                 input.getData(inputPitchBytes * (h - y)), inputPitchBytes,
                 w * mRaw->getBpp(), h - y);
      return;
    }
    if (BitOrder::MSB == order && bitPerPixel == 16) {
      decodePackedFP<BitPumpMSB, ieee_754_2008::Binary16>(size, offset,
                                                          skipBytes, h, y);
      return;
    }
    if (BitOrder::LSB == order && bitPerPixel == 16) {
      decodePackedFP<BitPumpLSB, ieee_754_2008::Binary16>(size, offset,
                                                          skipBytes, h, y);
      return;
    }
    if (BitOrder::MSB == order && bitPerPixel == 24) {
      decodePackedFP<BitPumpMSB, ieee_754_2008::Binary24>(size, offset,
                                                          skipBytes, h, y);
      return;
    }
    if (BitOrder::LSB == order && bitPerPixel == 24) {
      decodePackedFP<BitPumpLSB, ieee_754_2008::Binary24>(size, offset,
                                                          skipBytes, h, y);
      return;
    }
    ThrowRDE("Unsupported floating-point input bitwidth/bit packing: %u / %u",
             bitPerPixel, static_cast<unsigned>(order));
  }

  if (BitOrder::MSB == order) {
    decodePackedInt<BitPumpMSB>(size, offset, skipBytes, h, y, bitPerPixel);
  } else if (BitOrder::MSB16 == order) {
    decodePackedInt<BitPumpMSB16>(size, offset, skipBytes, h, y, bitPerPixel);
  } else if (BitOrder::MSB32 == order) {
    decodePackedInt<BitPumpMSB32>(size, offset, skipBytes, h, y, bitPerPixel);
  } else {
    if (bitPerPixel == 16 && getHostEndianness() == Endianness::little) {
      const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
      copyPixels(reinterpret_cast<uint8_t*>(&out(y, offset.x * cpp)), outPitch,
                 input.getData(inputPitchBytes * (h - y)), inputPitchBytes,
                 w * mRaw->getBpp(), h - y);
      return;
    }
    if (bitPerPixel == 12 && static_cast<int>(w) == inputPitchBytes * 8 / 12 &&
        getHostEndianness() == Endianness::little) {
      decode12BitRaw<Endianness::little>(w, h);
      return;
    }
    decodePackedInt<BitPumpLSB>(size, offset, skipBytes, h, y, bitPerPixel);
  }
}

template <bool uncorrectedRawValues>
void UncompressedDecompressor::decode8BitRaw(uint32_t w, uint32_t h) {
  sanityCheck(w, &h, 1);

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  const uint8_t* in = input.getData(w * h);
  uint32_t random = 0;
  for (uint32_t row = 0; row < h; row++) {
    for (uint32_t col = 0; col < w; col++) {
      if (uncorrectedRawValues)
        out(row, col) = *in;
      else
        mRaw->setWithLookUp(*in, reinterpret_cast<uint8_t*>(&out(row, col)),
                            &random);
      in++;
    }
  }
}

template void UncompressedDecompressor::decode8BitRaw<false>(uint32_t w,
                                                             uint32_t h);
template void UncompressedDecompressor::decode8BitRaw<true>(uint32_t w,
                                                            uint32_t h);

template <Endianness e, bool interlaced, bool skips>
void UncompressedDecompressor::decode12BitRaw(uint32_t w, uint32_t h) {
  static constexpr const auto bits = 12;

  static_assert(e == Endianness::little || e == Endianness::big,
                "unknown endianness");

  static constexpr const auto shift = 16 - bits;
  static constexpr const auto pack = 8 - shift;
  static constexpr const auto mask = (1 << pack) - 1;

  static_assert(bits == 12 && pack == 4, "wrong pack");

  static_assert(bits == 12 && mask == 0x0f, "wrong mask");

  uint32_t perline = bytesPerLine(w, skips);

  sanityCheck(&h, perline);

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  // FIXME: maybe check size of interlaced data?
  const uint8_t* in = input.peekData(perline * h);
  uint32_t half = (h + 1) >> 1;
  for (uint32_t row = 0; row < h; row++) {
    uint32_t y = !interlaced ? row : row % half * 2 + row / half;

    if (interlaced && y == 1) {
      // The second field starts at a 2048 byte alignment
      const uint32_t offset = ((half * w * 3 / 2 >> 11) + 1) << 11;
      input.skipBytes(offset);
      in = input.peekData(perline * (h - row));
    }

    for (uint32_t x = 0; x < w; x += 2) {
      uint32_t g1 = in[0];
      uint32_t g2 = in[1];

      auto process = [out, y](uint32_t i, bool invert, uint32_t p1,
                              uint32_t p2) {
        uint16_t pix;
        if (!(invert ^ (e == Endianness::little)))
          pix = (p1 << pack) | (p2 >> pack);
        else
          pix = ((p2 & mask) << 8) | p1;
        out(y, i) = pix;
      };

      process(x, false, g1, g2);

      g1 = in[2];

      process(x + 1, true, g1, g2);

      in += 3;

      if (skips && ((x % 10) == 8))
        in++;
    }
  }
  input.skipBytes(input.getRemainSize());
}

template void
UncompressedDecompressor::decode12BitRaw<Endianness::little, false, false>(
    uint32_t w, uint32_t h);
template void
UncompressedDecompressor::decode12BitRaw<Endianness::big, false, false>(
    uint32_t w, uint32_t h);
template void
UncompressedDecompressor::decode12BitRaw<Endianness::big, true, false>(
    uint32_t w, uint32_t h);
template void
UncompressedDecompressor::decode12BitRaw<Endianness::little, false, true>(
    uint32_t w, uint32_t h);
template void
UncompressedDecompressor::decode12BitRaw<Endianness::big, false, true>(
    uint32_t w, uint32_t h);

template <Endianness e>
void UncompressedDecompressor::decode12BitRawUnpackedLeftAligned(uint32_t w,
                                                                 uint32_t h) {
  static_assert(e == Endianness::big, "unknown endianness");

  sanityCheck(w, &h, 2);

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  const uint8_t* in = input.getData(w * h * 2);

  for (int row = 0; row < (int)h; row++) {
    for (int col = 0; col < (int)w; col += 1, in += 2) {
      uint32_t g1 = in[0];
      uint32_t g2 = in[1];

      if (e == Endianness::big)
        out(row, col) = (((g1 << 8) | (g2 & 0xf0)) >> 4);
    }
  }
}

template void
UncompressedDecompressor::decode12BitRawUnpackedLeftAligned<Endianness::big>(
    uint32_t w, uint32_t h);

template <int bits, Endianness e>
void UncompressedDecompressor::decodeRawUnpacked(uint32_t w, uint32_t h) {
  static_assert(bits == 12 || bits == 14 || bits == 16, "unhandled bitdepth");
  static_assert(e == Endianness::little || e == Endianness::big,
                "unknown endianness");

  static constexpr const auto shift = 16 - bits;
  static constexpr const auto mask = (1 << (8 - shift)) - 1;

  static_assert((bits == 12 && mask == 0x0f) || bits != 12, "wrong mask");
  static_assert((bits == 14 && mask == 0x3f) || bits != 14, "wrong mask");
  static_assert((bits == 16 && mask == 0xff) || bits != 16, "wrong mask");

  sanityCheck(w, &h, 2);

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  const uint8_t* in = input.getData(w * h * 2);

  for (int row = 0; row < (int)h; row++) {
    for (int col = 0; col < (int)w; col += 1, in += 2) {
      uint32_t g1 = in[0];
      uint32_t g2 = in[1];

      uint16_t pix;
      if (e == Endianness::little)
        pix = ((g2 << 8) | g1) >> shift;
      else
        pix = ((g1 & mask) << 8) | g2;
      out(row, col) = pix;
    }
  }
}

template void
UncompressedDecompressor::decodeRawUnpacked<12, Endianness::little>(uint32_t w,
                                                                    uint32_t h);
template void
UncompressedDecompressor::decodeRawUnpacked<12, Endianness::big>(uint32_t w,
                                                                 uint32_t h);
template void
UncompressedDecompressor::decodeRawUnpacked<14, Endianness::big>(uint32_t w,
                                                                 uint32_t h);
template void
UncompressedDecompressor::decodeRawUnpacked<16, Endianness::little>(uint32_t w,
                                                                    uint32_t h);
template void
UncompressedDecompressor::decodeRawUnpacked<16, Endianness::big>(uint32_t w,
                                                                 uint32_t h);

} // namespace rawspeed
