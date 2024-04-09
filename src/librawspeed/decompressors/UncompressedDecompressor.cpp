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
#include "adt/Array2DRef.h"
#include "adt/Casts.h"
#include "adt/Invariant.h"
#include "adt/Point.h"
#include "bitstreams/BitStreamerLSB.h"
#include "bitstreams/BitStreamerMSB.h"
#include "bitstreams/BitStreamerMSB16.h"
#include "bitstreams/BitStreamerMSB32.h"
#include "bitstreams/BitStreams.h"
#include "common/Common.h"
#include "common/FloatingPoint.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "io/IOException.h"
#include <algorithm>
#include <bit>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <utility>

using std::min;

namespace rawspeed {

void UncompressedDecompressor::sanityCheck(const uint32_t* h,
                                           int bytesPerLine) const {
  invariant(h != nullptr);
  invariant(*h > 0);
  invariant(bytesPerLine > 0);
  invariant(input.getSize() > 0);

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
  invariant(w > 0);
  invariant(bpp > 0);

  // bytes per line
  const auto bpl = bpp * w;
  invariant(bpl > 0);

  sanityCheck(h, bpl);
}

int UncompressedDecompressor::bytesPerLine(int w, bool skips) {
  invariant(w > 0);

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

UncompressedDecompressor::UncompressedDecompressor(
    ByteStream input_, RawImage img_, const iRectangle2D& crop,
    int inputPitchBytes_, int bitPerPixel_, BitOrder order_)
    : input(input_.getStream(crop.dim.y, inputPitchBytes_)),
      mRaw(std::move(img_)), size(crop.dim), offset(crop.pos),
      inputPitchBytes(inputPitchBytes_), bitPerPixel(bitPerPixel_),
      order(order_) {
  if (!size.hasPositiveArea())
    ThrowRDE("Empty tile.");

  if (inputPitchBytes < 1)
    ThrowRDE("Input pitch is non-positive");

  switch (order) {
  case BitOrder::LSB:
  case BitOrder::MSB:
  case BitOrder::MSB16:
  case BitOrder::MSB32:
    break;
  case BitOrder::JPEG:
    ThrowRDE("JPEG bit order not supported.");
  }

  uint32_t w = size.x;
  uint32_t h = size.y;
  uint32_t cpp = mRaw->getCpp();
  uint64_t ox = offset.x;
  uint64_t oy = offset.y;

  if (cpp < 1 || cpp > 3)
    ThrowRDE("Unsupported number of components per pixel: %u", cpp);

  if (bitPerPixel < 1 || bitPerPixel > 32 ||
      (bitPerPixel > 16 && mRaw->getDataType() == RawImageType::UINT16))
    ThrowRDE("Unsupported bit depth");

  const auto outPixelBits = static_cast<uint64_t>(w) * cpp * bitPerPixel;
  invariant(outPixelBits > 0);

  if (outPixelBits % 8 != 0) {
    ThrowRDE("Bad combination of cpp (%u), bps (%u) and width (%u), the "
             "pitch is %" PRIu64 " bits, which is not a multiple of 8 (1 byte)",
             cpp, bitPerPixel, w, outPixelBits);
  }

  const auto outPixelBytes = outPixelBits / 8;

  // The input pitch might be larger than needed (i.e. have some padding),
  // but it can *not* be smaller than needed.
  if (static_cast<unsigned>(inputPitchBytes) < outPixelBytes)
    ThrowRDE("Specified pitch is smaller than minimally-required pitch");

  // Check the specified pitch, not the minimally-required pitch.
  sanityCheck(&h, inputPitchBytes);

  invariant(static_cast<unsigned>(inputPitchBytes) >= outPixelBytes);
  skipBytes =
      implicit_cast<uint32_t>(inputPitchBytes - outPixelBytes); // Skip per line

  if (oy > static_cast<uint64_t>(mRaw->dim.y))
    ThrowRDE("Invalid y offset");
  if (ox + size.x > static_cast<uint64_t>(mRaw->dim.x))
    ThrowRDE("Invalid x offset");
}

template <typename Pump, typename NarrowFpType>
void UncompressedDecompressor::decodePackedFP(int rows, int row) const {
  const Array2DRef<float> out(mRaw->getF32DataAsUncroppedArray2DRef());
  Pump bits(input.peekRemainingBuffer().getAsArray1DRef());

  int cols = size.x * mRaw->getCpp();
  for (; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      uint32_t b = bits.getBits(NarrowFpType::StorageWidth);
      uint32_t f =
          extendBinaryFloatingPoint<NarrowFpType, ieee_754_2008::Binary32>(b);
      out(row, offset.x + col) = std::bit_cast<float>(f);
    }
    bits.skipBytes(skipBytes);
  }
}

template <typename Pump>
void UncompressedDecompressor::decodePackedInt(int rows, int row) const {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  Pump bits(input.peekRemainingBuffer().getAsArray1DRef());

  int cols = size.x * mRaw->getCpp();
  for (; row < rows; row++) {
    for (int x = 0; x < cols; x++) {
      out(row, x) = implicit_cast<uint16_t>(bits.getBits(bitPerPixel));
    }
    bits.skipBytes(skipBytes);
  }
}

void UncompressedDecompressor::readUncompressedRaw() {
  uint32_t outPitch = mRaw->pitch;
  uint32_t w = size.x;
  uint32_t h = size.y;
  uint32_t cpp = mRaw->getCpp();
  uint64_t oy = offset.y;

  uint64_t y = oy;
  h = implicit_cast<uint32_t>(min(h + oy, static_cast<uint64_t>(mRaw->dim.y)));

  if (mRaw->getDataType() == RawImageType::F32) {
    if (bitPerPixel == 32) {
      const Array2DRef<float> out(mRaw->getF32DataAsUncroppedArray2DRef());
      copyPixels(
          reinterpret_cast<std::byte*>(
              &out(implicit_cast<int>(y), offset.x * cpp)),
          outPitch,
          reinterpret_cast<const std::byte*>(input.getData(
              implicit_cast<Buffer::size_type>(inputPitchBytes * (h - y)))),
          inputPitchBytes, w * mRaw->getBpp(), implicit_cast<int>(h - y));
      return;
    }
    if (BitOrder::MSB == order && bitPerPixel == 16) {
      decodePackedFP<BitStreamerMSB, ieee_754_2008::Binary16>(
          h, implicit_cast<int>(y));
      return;
    }
    if (BitOrder::LSB == order && bitPerPixel == 16) {
      decodePackedFP<BitStreamerLSB, ieee_754_2008::Binary16>(
          h, implicit_cast<int>(y));
      return;
    }
    if (BitOrder::MSB == order && bitPerPixel == 24) {
      decodePackedFP<BitStreamerMSB, ieee_754_2008::Binary24>(
          h, implicit_cast<int>(y));
      return;
    }
    if (BitOrder::LSB == order && bitPerPixel == 24) {
      decodePackedFP<BitStreamerLSB, ieee_754_2008::Binary24>(
          h, implicit_cast<int>(y));
      return;
    }
    ThrowRDE("Unsupported floating-point input bitwidth/bit packing: %u / %u",
             bitPerPixel, static_cast<unsigned>(order));
  }

  if (BitOrder::MSB == order) {
    decodePackedInt<BitStreamerMSB>(h, implicit_cast<int>(y));
  } else if (BitOrder::MSB16 == order) {
    decodePackedInt<BitStreamerMSB16>(h, implicit_cast<int>(y));
  } else if (BitOrder::MSB32 == order) {
    decodePackedInt<BitStreamerMSB32>(h, implicit_cast<int>(y));
  } else {
    if (bitPerPixel == 16 && getHostEndianness() == Endianness::little) {
      const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
      copyPixels(
          reinterpret_cast<std::byte*>(
              &out(implicit_cast<int>(y), offset.x * cpp)),
          outPitch,
          reinterpret_cast<const std::byte*>(input.getData(
              implicit_cast<Buffer::size_type>(inputPitchBytes * (h - y)))),
          inputPitchBytes, w * mRaw->getBpp(), implicit_cast<int>(h - y));
      return;
    }
    decodePackedInt<BitStreamerLSB>(h, implicit_cast<int>(y));
  }
}

template <bool uncorrectedRawValues>
void UncompressedDecompressor::decode8BitRaw() {
  uint32_t w = size.x;
  uint32_t h = size.y;
  sanityCheck(w, &h, 1);

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  const auto in = Array2DRef(input.getData(w * h), w, h);
  uint32_t random = 0;
  for (uint32_t row = 0; row < h; row++) {
    for (uint32_t col = 0; col < w; col++) {
      if constexpr (uncorrectedRawValues)
        out(row, col) = in(row, col);
      else {
        mRaw->setWithLookUp(in(row, col),
                            reinterpret_cast<std::byte*>(&out(row, col)),
                            &random);
      }
    }
  }
}

template void UncompressedDecompressor::decode8BitRaw<false>();
template void UncompressedDecompressor::decode8BitRaw<true>();

template <Endianness e>
void UncompressedDecompressor::decode12BitRawWithControl() {
  uint32_t w = size.x;
  uint32_t h = size.y;
  static constexpr const auto bits = 12;

  static_assert(e == Endianness::little || e == Endianness::big,
                "unknown endianness");

  static constexpr const auto shift = 16 - bits;
  static constexpr const auto pack = 8 - shift;
  static constexpr const auto mask = (1 << pack) - 1;

  static_assert(bits == 12 && pack == 4, "wrong pack");

  static_assert(bits == 12 && mask == 0x0f, "wrong mask");

  uint32_t perline = bytesPerLine(w, /*skips=*/true);

  sanityCheck(&h, perline);

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  const auto in = Array2DRef(input.getData(perline * h), perline, h);
  for (uint32_t row = 0; row < h; row++) {
    uint32_t col = 0;
    for (uint32_t x = 0; x < w; x += 2) {
      uint32_t g1 = in(row, col + 0);
      uint32_t g2 = in(row, col + 1);

      auto process = [out, row](uint32_t i, bool invert, uint32_t p1,
                                uint32_t p2) {
        uint16_t pix;
        if (!(invert ^ (e == Endianness::little)))
          pix = implicit_cast<uint16_t>((p1 << pack) | (p2 >> pack));
        else
          pix = implicit_cast<uint16_t>(((p2 & mask) << 8) | p1);
        out(row, i) = pix;
      };

      process(x, false, g1, g2);

      g1 = in(row, col + 2);

      process(x + 1, true, g1, g2);

      col += 3;

      if ((x % 10) == 8)
        col++;
    }
  }
  input.skipBytes(input.getRemainSize());
}

template void
UncompressedDecompressor::decode12BitRawWithControl<Endianness::little>();
template void
UncompressedDecompressor::decode12BitRawWithControl<Endianness::big>();

template <Endianness e>
void UncompressedDecompressor::decode12BitRawUnpackedLeftAligned() {
  uint32_t w = size.x;
  uint32_t h = size.y;
  sanityCheck(w, &h, 2);

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  const auto in = Array2DRef(input.getData(w * h * 2), 2 * w, h);

  for (int row = 0; row < static_cast<int>(h); row++) {
    for (int col = 0; col < static_cast<int>(w); ++col) {
      uint32_t g1 = in(row, 2 * col + 0);
      uint32_t g2 = in(row, 2 * col + 1);

      uint16_t pix;
      if constexpr (e == Endianness::little)
        pix = implicit_cast<uint16_t>((g2 << 8) | g1);
      else
        pix = implicit_cast<uint16_t>((g1 << 8) | g2);
      out(row, col) = pix >> 4;
    }
  }
}

template void
UncompressedDecompressor::decode12BitRawUnpackedLeftAligned<Endianness::big>();
template void UncompressedDecompressor::decode12BitRawUnpackedLeftAligned<
    Endianness::little>();

} // namespace rawspeed
