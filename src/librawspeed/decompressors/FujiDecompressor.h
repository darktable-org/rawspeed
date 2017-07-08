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

#include "common/Common.h"                      // for uint64, uchar8, usho...
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/ByteStream.h"                      // for ByteStream
#include "metadata/ColorFilterArray.h"          // for CFAColor
#include <array>                                // for array
#include <vector>                               // for vector

namespace rawspeed {

class FujiDecompressor final : public AbstractDecompressor {
public:
  FujiDecompressor(ByteStream input, const RawImage& img);

  using ushort = ushort16;

  void fuji_compressed_load_raw();

protected:
  struct fuji_compressed_params {
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
    int cur_bit;            // current bit being read (from left to right)
    int cur_pos;            // current position in a buffer
    uint64 cur_buf_offset;  // offset of this buffer in a file
    unsigned max_read_size; // Amount of data to be read
    int cur_buf_size;       // buffer size
    const uchar8* cur_buf;  // currently read block
    int fillbytes;          // Counter to add extra byte for block size N*16
    struct int_pair grad_even[3][41]; // tables of gradients
    struct int_pair grad_odd[3][41];
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
  int data_offset = 0;

  void parse_fuji_compressed_header();
  void init_fuji_compr(struct fuji_compressed_params* info);
  void fuji_decode_loop(const struct fuji_compressed_params* common_info,
                        int count, uint64* raw_block_offsets,
                        unsigned* block_sizes);
  void fuji_decode_strip(const struct fuji_compressed_params* info_common,
                         int cur_block, uint64 raw_offset, unsigned dsize);
  void init_fuji_block(struct fuji_compressed_block* info,
                       const struct fuji_compressed_params* params,
                       uint64 raw_offset, unsigned dsize);
  void fuji_fill_buffer(struct fuji_compressed_block* info);

  template <typename T>
  void copy_line(struct fuji_compressed_block* info, int cur_line,
                 int cur_block, int cur_block_width, T&& idx);

  void copy_line_to_xtrans(struct fuji_compressed_block* info, int cur_line,
                           int cur_block, int cur_block_width);
  void copy_line_to_bayer(struct fuji_compressed_block* info, int cur_line,
                          int cur_block, int cur_block_width);
  void fuji_zerobits(struct fuji_compressed_block* info, int* count);
  void fuji_read_code(struct fuji_compressed_block* info, int* data,
                      int bits_to_read);
  int bitDiff(int value1, int value2);
  int fuji_decode_sample_even(struct fuji_compressed_block* info,
                              const struct fuji_compressed_params* params,
                              ushort* line_buf, int pos,
                              struct int_pair* grads);
  int fuji_decode_sample_odd(struct fuji_compressed_block* info,
                             const struct fuji_compressed_params* params,
                             ushort* line_buf, int pos, struct int_pair* grads);
  void fuji_decode_interpolation_even(int line_width, ushort* line_buf,
                                      int pos);
  void fuji_extend_generic(ushort* linebuf[_ltotal], int line_width, int start,
                           int end);
  void fuji_extend_red(ushort* linebuf[_ltotal], int line_width);
  void fuji_extend_green(ushort* linebuf[_ltotal], int line_width);
  void fuji_extend_blue(ushort* linebuf[_ltotal], int line_width);
  void xtrans_decode_block(struct fuji_compressed_block* info,
                           const struct fuji_compressed_params* params,
                           int cur_line);
  void fuji_bayer_decode_block(struct fuji_compressed_block* info,
                               const struct fuji_compressed_params* params,
                               int cur_line);
};

} // namespace rawspeed
