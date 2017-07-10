/*
    FujiDecompressor - Decompress Fujifilm compressed RAF.

    Copyright (C) 2017 Uwe Müssel

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

#include "common/Common.h"                      // for ushort16
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/BitPumpMSB.h"                      // for BitPumpMSB
#include "io/ByteStream.h"                      // for ByteStream
#include "metadata/ColorFilterArray.h"          // for CFAColor
#include <array>                                // for array
#include <vector>                               // for vector

namespace rawspeed {

class FujiDecompressor final : public AbstractDecompressor {
public:
  using ushort = ushort16;

  struct FujiHeader {
    explicit FujiHeader(ByteStream* input_);
    explicit operator bool() const; // validity check

    ushort signature;
    uchar8 version;
    uchar8 raw_type;
    uchar8 raw_bits;
    ushort raw_height;
    ushort raw_rounded_width;
    ushort raw_width;
    ushort block_size;
    uchar8 blocks_in_row;
    ushort total_lines;
  };

  FujiDecompressor(ByteStream input, const RawImage& img);

  void fuji_compressed_load_raw();

protected:
  struct fuji_compressed_params {
    explicit fuji_compressed_params(const FujiDecompressor& d);

    std::vector<char> q_table; /* quantization table */
    int q_point[5]; /* quantization points */
    int max_bits;
    int min_value;
    int raw_bits;
    int total_values;
    int maxDiff;
    ushort line_width;
  };

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
    fuji_compressed_block(const fuji_compressed_params* params,
                          const ByteStream& strip);

    BitPumpMSB pump;

    int_pair grad_even[3][41]; // tables of gradients
    int_pair grad_odd[3][41];
    std::vector<ushort> linealloc;
    ushort* linebuf[_ltotal];
  };

private:
  ByteStream input;
  RawImage mImg;

  std::array<std::array<CFAColor, 6>, 6> CFA;

  int fuji_total_lines;
  int fuji_total_blocks;
  int fuji_block_width;
  int fuji_bits;
  int fuji_raw_type;
  int raw_width;
  int raw_height;

  void fuji_decode_loop(const fuji_compressed_params* common_info,
                        std::vector<ByteStream> strips);
  void fuji_decode_strip(const fuji_compressed_params* info_common,
                         int cur_block, const ByteStream& strip);

  template <typename T>
  void copy_line(fuji_compressed_block* info, int cur_line, int cur_block,
                 int cur_block_width, T&& idx);

  void copy_line_to_xtrans(fuji_compressed_block* info, int cur_line,
                           int cur_block, int cur_block_width);
  void copy_line_to_bayer(fuji_compressed_block* info, int cur_line,
                          int cur_block, int cur_block_width);
  void fuji_zerobits(fuji_compressed_block* info, int* count);
  int bitDiff(int value1, int value2);
  int fuji_decode_sample_even(fuji_compressed_block* info,
                              const fuji_compressed_params* params,
                              ushort* line_buf, int pos, int_pair* grads);
  int fuji_decode_sample_odd(fuji_compressed_block* info,
                             const fuji_compressed_params* params,
                             ushort* line_buf, int pos, int_pair* grads);
  void fuji_decode_interpolation_even(int line_width, ushort* line_buf,
                                      int pos);
  void fuji_extend_generic(ushort* linebuf[_ltotal], int line_width, int start,
                           int end);
  void fuji_extend_red(ushort* linebuf[_ltotal], int line_width);
  void fuji_extend_green(ushort* linebuf[_ltotal], int line_width);
  void fuji_extend_blue(ushort* linebuf[_ltotal], int line_width);
  void xtrans_decode_block(fuji_compressed_block* info,
                           const fuji_compressed_params* params, int cur_line);
  void fuji_bayer_decode_block(fuji_compressed_block* info,
                               const fuji_compressed_params* params,
                               int cur_line);
};

} // namespace rawspeed
