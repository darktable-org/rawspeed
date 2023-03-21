/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2019 LibRaw LLC (info@libraw.org)
    Copyright (C) 2020 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "rawspeedconfig.h" // for HAVE_OPENMP
#include "decompressors/PanasonicV6Decompressor.h"
#include "adt/Array2DRef.h"               // for Array2DRef
#include "adt/Invariant.h"                // for invariant
#include "adt/Point.h"                    // for iPoint2D
#include "common/Common.h"                // for rawspeed_get_number_of_pro...
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowException, ThrowRDE
#include "io/BitPumpLSB.h"                // for BitPumpLSB
#include <array>                          // for array
#include <cstdint>                        // for uint16_t, uint32_t

namespace rawspeed {

struct PanasonicV6Decompressor::BlockDsc {
  int BitsPerSample;
  bool is14Bit;

  int PixelsPerBlock;
  unsigned PixelbaseZero;
  unsigned PixelbaseCompare;
  unsigned SpixCompare;
  unsigned PixelMask;
  const int BytesPerBlock = 16;

  constexpr explicit BlockDsc(int bps_)
      : BitsPerSample(bps_), is14Bit(BitsPerSample == 14),
        PixelsPerBlock(is14Bit ? 11 : 14),
        PixelbaseZero(is14Bit ? 0x200 : 0x80),
        PixelbaseCompare(is14Bit ? 0x2000 : 0x800),
        SpixCompare(is14Bit ? 0xffff : 0x3fff),
        PixelMask(is14Bit ? 0x3fff : 0xfff) {
    invariant((BitsPerSample == 14 || BitsPerSample == 12) &&
              "invalid bits per sample), only use 12/14 bits.");
  }
};

constexpr PanasonicV6Decompressor::BlockDsc
    PanasonicV6Decompressor::TwelveBitBlock =
        PanasonicV6Decompressor::BlockDsc(/*bps=*/12);
constexpr PanasonicV6Decompressor::BlockDsc
    PanasonicV6Decompressor::FourteenBitBlock =
        PanasonicV6Decompressor::BlockDsc(/*bps=*/14);

namespace {
template <int B> struct pana_cs6_page_decoder {
  static_assert((B == 14 || B == 12), "only 12/14 bits are valid!");
  static constexpr int BufferSize = B == 14 ? 14 : 18;
  std::array<uint16_t, BufferSize> pixelbuffer;
  unsigned char current = 0;

  void fillBuffer(ByteStream bs) noexcept;

  explicit pana_cs6_page_decoder(ByteStream bs) { fillBuffer(bs); }

  uint16_t nextpixel() {
    uint16_t currPixel = pixelbuffer[current];
    ++current;
    return currPixel;
  }
};

template <>
inline void __attribute__((always_inline))
pana_cs6_page_decoder<12>::fillBuffer(ByteStream bs_) noexcept {
  BitPumpLSB bs(bs_);
  bs.fill(32);
  pixelbuffer[17] = bs.getBits(8);
  pixelbuffer[16] = bs.getBits(8);
  pixelbuffer[15] = bs.getBits(8);
  pixelbuffer[14] = bs.getBits(2);
  pixelbuffer[13] = bs.getBits(8);
  pixelbuffer[12] = bs.getBits(8);
  pixelbuffer[11] = bs.getBits(8);
  pixelbuffer[10] = bs.getBits(2);
  pixelbuffer[9] = bs.getBits(8);
  pixelbuffer[8] = bs.getBits(8);
  pixelbuffer[7] = bs.getBits(8);
  pixelbuffer[6] = bs.getBits(2);
  pixelbuffer[5] = bs.getBits(8);
  pixelbuffer[4] = bs.getBits(8);
  pixelbuffer[3] = bs.getBits(8);
  pixelbuffer[2] = bs.getBits(2);
  pixelbuffer[1] = bs.getBits(12);
  pixelbuffer[0] = bs.getBits(12);
}

template <>
inline void __attribute__((always_inline))
pana_cs6_page_decoder<14>::fillBuffer(ByteStream bs_) noexcept {
  BitPumpLSB bs(bs_);
  bs.fill(32);
  bs.skipBitsNoFill(4);
  pixelbuffer[13] = bs.getBits(10);
  pixelbuffer[12] = bs.getBits(10);
  pixelbuffer[11] = bs.getBits(10);
  pixelbuffer[10] = bs.getBits(2);
  pixelbuffer[9] = bs.getBits(10);
  pixelbuffer[8] = bs.getBits(10);
  pixelbuffer[7] = bs.getBits(10);
  pixelbuffer[6] = bs.getBits(2);
  pixelbuffer[5] = bs.getBits(10);
  pixelbuffer[4] = bs.getBits(10);
  pixelbuffer[3] = bs.getBits(10);
  pixelbuffer[2] = bs.getBits(2);
  pixelbuffer[1] = bs.getBits(14);
  pixelbuffer[0] = bs.getBits(14);
}

} // namespace

PanasonicV6Decompressor::PanasonicV6Decompressor(const RawImage& img,
                                                 ByteStream input_,
                                                 uint32_t bps_)
    : mRaw(img), bps(bps_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  const BlockDsc* dsc = nullptr;
  switch (bps) {
  case 12:
    dsc = &TwelveBitBlock;
    break;
  case 14:
    dsc = &FourteenBitBlock;
    break;
  default:
    ThrowRDE("Unsupported bps: %u", bps);
  }

  if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % dsc->PixelsPerBlock != 0) {
    ThrowRDE("Unexpected image dimensions found: (%i; %i)", mRaw->dim.x,
             mRaw->dim.y);
  }

  // How many blocks are needed for the given image size?
  const auto numBlocks = mRaw->dim.area() / dsc->PixelsPerBlock;

  // Does the input contain enough blocks?
  // How many full blocks does the input contain? This is truncating division.
  if (const auto haveBlocks = input_.getRemainSize() / dsc->BytesPerBlock;
      haveBlocks < numBlocks)
    ThrowRDE("Insufficient count of input blocks for a given image");

  // We only want those blocks we need, no extras.
  input = input_.peekStream(numBlocks, dsc->BytesPerBlock);
}

template <const PanasonicV6Decompressor::BlockDsc& dsc>
inline void __attribute__((always_inline))
// NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
PanasonicV6Decompressor::decompressBlock(ByteStream& rowInput, int row,
                                         int col) const noexcept {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  pana_cs6_page_decoder<dsc.BitsPerSample> page(
      rowInput.getStream(dsc.BytesPerBlock));

  std::array<unsigned, 2> oddeven = {0, 0};
  std::array<unsigned, 2> nonzero = {0, 0};
  unsigned pmul = 0;
  unsigned pixel_base = 0;
  for (int pix = 0; pix < dsc.PixelsPerBlock; pix++, col++) {
    if (pix % 3 == 2) {
      uint16_t base = page.nextpixel();
      if (base == 3)
        base = 4;
      pixel_base = dsc.PixelbaseZero << base;
      pmul = 1 << base;
    }
    uint16_t epixel = page.nextpixel();
    if (oddeven[pix % 2]) {
      epixel *= pmul;
      if (pixel_base < dsc.PixelbaseCompare && nonzero[pix % 2] > pixel_base)
        epixel += nonzero[pix % 2] - pixel_base;
      nonzero[pix % 2] = epixel;
    } else {
      oddeven[pix % 2] = epixel;
      if (epixel)
        nonzero[pix % 2] = epixel;
      else
        epixel = nonzero[pix % 2];
    }
    auto spix = static_cast<unsigned>(static_cast<int>(epixel) - 0xf);
    if (spix <= dsc.SpixCompare)
      out(row, col) = spix & dsc.SpixCompare;
    else {
      // FIXME: this is a convoluted way to compute zero.
      // What was this code trying to do, actually?
      epixel = static_cast<int>(epixel + 0x7ffffff1) >> 0x1f;
      out(row, col) = epixel & dsc.PixelMask;
    }
  }
}

// NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
template <const PanasonicV6Decompressor::BlockDsc& dsc>
void PanasonicV6Decompressor::decompressRow(int row) const noexcept {
  invariant(mRaw->dim.x % dsc.PixelsPerBlock == 0);
  const int blocksperrow = mRaw->dim.x / dsc.PixelsPerBlock;
  const int bytesPerRow = dsc.BytesPerBlock * blocksperrow;

  ByteStream rowInput = input.getSubStream(bytesPerRow * row, bytesPerRow);
  for (int rblock = 0, col = 0; rblock < blocksperrow;
       rblock++, col += dsc.PixelsPerBlock)
    decompressBlock<dsc>(rowInput, row, col);
}

template <const PanasonicV6Decompressor::BlockDsc& dsc>
void PanasonicV6Decompressor::decompressInternal() const noexcept {
#ifdef HAVE_OPENMP
#pragma omp parallel for num_threads(rawspeed_get_number_of_processor_cores()) \
    schedule(static) default(none)
#endif
  for (int row = 0; row < mRaw->dim.y;
       ++row) { // NOLINT(openmp-exception-escape): we know no exceptions will
                // be thrown.
    decompressRow<dsc>(row);
  }
}

void PanasonicV6Decompressor::decompress() const noexcept {
  switch (bps) {
  case 12:
    decompressInternal<TwelveBitBlock>();
    break;
  case 14:
    decompressInternal<FourteenBitBlock>();
    break;
  default:
    __builtin_unreachable();
  }
}

} // namespace rawspeed
