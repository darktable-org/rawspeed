/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Uwe Müssel
    Copyright (C) 2017 Roman Lebedev

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

#pragma once

#include "common/Common.h"                      // for uint16_t
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/BitPumpMSB.h"                      // for BitPumpMSB
#include "io/ByteStream.h"                      // for ByteStream
#include "metadata/ColorFilterArray.h"          // for CFAColor
#include <array>                                // for array
#include <cassert>                              // for assert
#include <utility>                              // for move
#include <vector>                               // for vector

namespace rawspeed {

class RawImage;

class FujiDecompressor final : public AbstractDecompressor {
  RawImage mRaw;

  void decompressThread() const noexcept;

public:
  struct FujiHeader {
    FujiHeader() = default;

    explicit FujiHeader(ByteStream* input_);
    explicit __attribute__((pure)) operator bool() const; // validity check

    uint16_t signature;
    uint8_t version;
    uint8_t raw_type;
    uint8_t raw_bits;
    uint16_t raw_height;
    uint16_t raw_rounded_width;
    uint16_t raw_width;
    uint16_t block_size;
    uint8_t blocks_in_row;
    uint16_t total_lines;
  };

  FujiHeader header;

  struct FujiStrip {
    // part of which 'image' this block is
    const FujiHeader& h;

    // which strip is this, 0 .. h.blocks_in_row-1
    const int n;

    // the compressed data of this strip
    const ByteStream bs;

    FujiStrip(const FujiHeader& h_, int block, ByteStream bs_)
        : h(h_), n(block), bs(std::move(bs_)) {
      assert(n >= 0 && n < h.blocks_in_row);
    }

    // each strip's line corresponds to 6 output lines.
    static int lineHeight() { return 6; }

    // how many vertical lines does this block encode?
    int height() const { return h.total_lines; }

    // how many horizontal pixels does this block encode?
    int width() const {
      // if this is not the last block, we are good.
      if ((n + 1) != h.blocks_in_row)
        return h.block_size;

      // ok, this is the last block...

      assert(h.block_size * h.blocks_in_row >= h.raw_width);
      return h.raw_width - offsetX();
    }

    // where vertically does this block start?
    int offsetY(int line = 0) const {
      assert(line >= 0 && line < height());
      return lineHeight() * line;
    }

    // where horizontally does this block start?
    int offsetX() const { return h.block_size * n; }
  };

  FujiDecompressor(const RawImage& img, ByteStream input);

  void fuji_compressed_load_raw();

  void decompress() const;

protected:
  struct fuji_compressed_params {
    fuji_compressed_params() = default;

    explicit fuji_compressed_params(const FujiDecompressor& d);

    std::vector<char> q_table; /* quantization table */
    std::array<int, 5> q_point; /* quantization points */
    int max_bits;
    int min_value;
    int raw_bits;
    int total_values;
    int maxDiff;
    uint16_t line_width;
  };

  fuji_compressed_params common_info;

  struct int_pair {
    int value1;
    int value2;
  };

  enum _xt_lines {
    _R0 = 0,
    _R1,
    _R2,
    _R3,
    _R4,
    _G0,
    _G1,
    _G2,
    _G3,
    _G4,
    _G5,
    _G6,
    _G7,
    _B0,
    _B1,
    _B2,
    _B3,
    _B4,
    _ltotal
  };

  struct fuji_compressed_block {
    fuji_compressed_block() = default;

    void reset(const fuji_compressed_params* params);

    BitPumpMSB pump;

    // tables of gradients
    std::array<std::array<int_pair, 41>, 3> grad_even;
    std::array<std::array<int_pair, 41>, 3> grad_odd;

    std::vector<uint16_t> linealloc;
    std::array<uint16_t*, _ltotal> linebuf;
  };

private:
  ByteStream input;

  std::array<std::array<CFAColor, 6>, 6> CFA;

  std::vector<FujiStrip> strips;

  void fuji_decode_strip(fuji_compressed_block* info_block,
                         const FujiStrip& strip) const;

  template <typename T>
  void copy_line(fuji_compressed_block* info, const FujiStrip& strip,
                 int cur_line, T&& idx) const;

  void copy_line_to_xtrans(fuji_compressed_block* info, const FujiStrip& strip,
                           int cur_line) const;
  void copy_line_to_bayer(fuji_compressed_block* info, const FujiStrip& strip,
                          int cur_line) const;

  static inline void fuji_zerobits(BitPumpMSB* pump, int* count);
  static int bitDiff(int value1, int value2);

  template <typename T1, typename T2>
  void fuji_decode_sample(T1&& func_0, T2&& func_1, fuji_compressed_block* info,
                          uint16_t* line_buf, int* pos,
                          std::array<int_pair, 41>* grads) const;
  void fuji_decode_sample_even(fuji_compressed_block* info, uint16_t* line_buf,
                               int* pos, std::array<int_pair, 41>* grads) const;
  void fuji_decode_sample_odd(fuji_compressed_block* info, uint16_t* line_buf,
                              int* pos, std::array<int_pair, 41>* grads) const;

  static void fuji_decode_interpolation_even(int line_width, uint16_t* line_buf,
                                             int* pos);
  static void fuji_extend_generic(std::array<uint16_t*, _ltotal> linebuf,
                                  int line_width, int start, int end);
  static void fuji_extend_red(std::array<uint16_t*, _ltotal> linebuf,
                              int line_width);
  static void fuji_extend_green(std::array<uint16_t*, _ltotal> linebuf,
                                int line_width);
  static void fuji_extend_blue(std::array<uint16_t*, _ltotal> linebuf,
                               int line_width);
  void xtrans_decode_block(fuji_compressed_block* info, int cur_line) const;
  void fuji_bayer_decode_block(fuji_compressed_block* info, int cur_line) const;
};

} // namespace rawspeed
