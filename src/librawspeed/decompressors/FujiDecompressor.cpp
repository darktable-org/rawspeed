/*
    FujiDecompressor - Decompress Fujifilm compressed RAF.

    Copyright (C) 2016 Alexey Danilchenko
    Copyright (C) 2016 Alex Tutubalin
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

#include "decompressors/FujiDecompressor.h"
#include "decoders/RawDecoderException.h" // for RawDecoderException (ptr o...
#include "io/Endianness.h"                // for Endianness::big
#include "metadata/ColorFilterArray.h"    // for CFAColor::CFA_BLUE, CFACol...
#include <algorithm>                      // for min, move
#include <cstdlib>                        // for abs
#include <cstring>                        // for memcpy, memset
// IWYU pragma: no_include <bits/std_abs.h>

namespace rawspeed {

FujiDecompressor::FujiDecompressor(ByteStream input_, const RawImage& img)
    : input(std::move(input_)), mImg(img) {
  input.setByteOrder(big);

  header = FujiHeader(&input);
  if (!header)
    ThrowRDE("compressed RAF header check");

  if (12 == header.raw_bits) {
    ThrowRDE("Aha, finally, a 12-bit compressed RAF! Please consider providing "
             "samples on <https://raw.pixls.us/>, thanks!");
  }

  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 6; j++)
      CFA[i][j] = mImg->cfa.getColorAt(j, i);
  }
}

FujiDecompressor::fuji_compressed_params::fuji_compressed_params(
    const FujiDecompressor& d) {
  int cur_val;
  char* qt;

  if ((d.header.block_size % 3 && d.header.raw_type == 16) ||
      (d.header.block_size & 1 && d.header.raw_type == 0)) {
    ThrowRDE("fuji_block_checks");
  }

  q_table.resize(32768);

  if (d.header.raw_type == 16) {
    line_width = (d.header.block_size * 2) / 3;
  } else {
    line_width = d.header.block_size >> 1;
  }

  q_point[0] = 0;
  q_point[1] = 0x12;
  q_point[2] = 0x43;
  q_point[3] = 0x114;
  q_point[4] = (1 << d.header.raw_bits) - 1;
  min_value = 0x40;

  cur_val = -q_point[4];

  for (qt = &q_table[0]; cur_val <= q_point[4]; ++qt, ++cur_val) {
    if (cur_val <= -q_point[3]) {
      *qt = -4;
    } else if (cur_val <= -q_point[2]) {
      *qt = -3;
    } else if (cur_val <= -q_point[1]) {
      *qt = -2;
    } else if (cur_val < 0) {
      *qt = -1;
    } else if (cur_val == 0) {
      *qt = 0;
    } else if (cur_val < q_point[1]) {
      *qt = 1;
    } else if (cur_val < q_point[2]) {
      *qt = 2;
    } else if (cur_val < q_point[3]) {
      *qt = 3;
    } else {
      *qt = 4;
    }
  }

  // populting gradients
  if (q_point[4] == 0x3FFF) {
    total_values = 0x4000;
    raw_bits = 14;
    max_bits = 56;
    maxDiff = 256;
  } else if (q_point[4] == 0xFFF) {
    ThrowRDE("Aha, finally, a 12-bit compressed RAF! Please consider providing "
             "samples on <https://raw.pixls.us/>, thanks!");

    total_values = 4096;
    raw_bits = 12;
    max_bits = 48;
    maxDiff = 64;
  } else {
    ThrowRDE("FUJI q_point");
  }
}

void FujiDecompressor::fuji_compressed_block::reset(
    const fuji_compressed_params* params) {
  const bool reInit = !linealloc.empty();

  linealloc.resize(_ltotal * (params->line_width + 2), 0);

  if (reInit)
    std::fill(linealloc.begin(), linealloc.end(), 0);

  linebuf[_R0] = &linealloc[0];

  for (int i = _R1; i <= _B4; i++) {
    linebuf[i] = linebuf[i - 1] + params->line_width + 2;
  }

  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < 41; i++) {
      grad_even[j][i].value1 = params->maxDiff;
      grad_even[j][i].value2 = 1;
      grad_odd[j][i].value1 = params->maxDiff;
      grad_odd[j][i].value2 = 1;
    }
  }
}

template <typename T>
void FujiDecompressor::copy_line(fuji_compressed_block* info, int cur_line,
                                 int cur_block, int cur_block_width, T&& idx) {
  ushort16* lineBufB[3];
  ushort16* lineBufG[6];
  ushort16* lineBufR[3];

  for (int i = 0; i < 3; i++) {
    lineBufR[i] = info->linebuf[_R2 + i] + 1;
    lineBufB[i] = info->linebuf[_B2 + i] + 1;
  }

  for (int i = 0; i < 6; i++) {
    lineBufG[i] = info->linebuf[_G2 + i] + 1;
  }

  int row_count = 0;
  while (row_count < 6) {
    auto* const raw_block_data = reinterpret_cast<ushort16*>(
        mImg->getData(header.block_size * cur_block, 6 * cur_line + row_count));

    int pixel_count = 0;
    while (pixel_count < cur_block_width) {
      ushort16* line_buf = nullptr;

      switch (CFA[row_count][pixel_count % 6]) {
      case CFA_RED: // red
        line_buf = lineBufR[row_count >> 1];
        break;

      case CFA_GREEN: // green
        line_buf = lineBufG[row_count];
        break;

      case CFA_BLUE: // blue
        line_buf = lineBufB[row_count >> 1];
        break;

      default:
        __builtin_unreachable();
      }

      raw_block_data[pixel_count] = line_buf[idx(pixel_count)];

      ++pixel_count;
    }

    ++row_count;
  }
}

void FujiDecompressor::copy_line_to_xtrans(fuji_compressed_block* info,
                                           int cur_line, int cur_block,
                                           int cur_block_width) {
  auto index = [](int pixel_count) {
    return (((pixel_count * 2 / 3) & 0x7FFFFFFE) | ((pixel_count % 3) & 1)) +
           ((pixel_count % 3) >> 1);
  };

  copy_line(info, cur_line, cur_block, cur_block_width, index);
}

void FujiDecompressor::copy_line_to_bayer(fuji_compressed_block* info,
                                          int cur_line, int cur_block,
                                          int cur_block_width) {
  auto index = [](int pixel_count) { return pixel_count >> 1; };

  copy_line(info, cur_line, cur_block, cur_block_width, index);
}

#define fuji_quant_gradient(i, v1, v2)                                         \
  (9 * (i)->q_table[(i)->q_point[4] + (v1)] +                                  \
   (i)->q_table[(i)->q_point[4] + (v2)])

void FujiDecompressor::fuji_zerobits(BitPumpMSB* pump, int* count) {
  uchar8 zero = 0;
  *count = 0;

  while (zero == 0) {
    zero = pump->getBits(1);

    if (zero)
      break;

    ++*count;
  }
}

int __attribute__((const)) FujiDecompressor::bitDiff(int value1, int value2) {
  int decBits = 0;

  if (value2 >= value1)
    return decBits;

  while (decBits <= 12) {
    ++decBits;

    if ((value2 << decBits) >= value1)
      return decBits;
  }

  return decBits;
}

int FujiDecompressor::fuji_decode_sample_even(
    fuji_compressed_block* info, const fuji_compressed_params* params,
    BitPumpMSB* pump, ushort16* line_buf, int pos, int_pair* grads) {
  int interp_val = 0;
  int errcnt = 0;

  int sample = 0;
  int code = 0;
  ushort16* line_buf_cur = line_buf + pos;
  int Rb = line_buf_cur[-2 - params->line_width];
  int Rc = line_buf_cur[-3 - params->line_width];
  int Rd = line_buf_cur[-1 - params->line_width];
  int Rf = line_buf_cur[-4 - 2 * params->line_width];

  int grad;
  int gradient;
  int diffRcRb;
  int diffRfRb;
  int diffRdRb;

  grad = fuji_quant_gradient(params, Rb - Rf, Rc - Rb);
  gradient = std::abs(grad);
  diffRcRb = std::abs(Rc - Rb);
  diffRfRb = std::abs(Rf - Rb);
  diffRdRb = std::abs(Rd - Rb);

  if (diffRcRb > diffRfRb && diffRcRb > diffRdRb) {
    interp_val = Rf + Rd + 2 * Rb;
  } else if (diffRdRb > diffRcRb && diffRdRb > diffRfRb) {
    interp_val = Rf + Rc + 2 * Rb;
  } else {
    interp_val = Rd + Rc + 2 * Rb;
  }

  fuji_zerobits(pump, &sample);

  if (sample < params->max_bits - params->raw_bits - 1) {
    int decBits = bitDiff(grads[gradient].value1, grads[gradient].value2);
    code = pump->getBits(decBits);
    code += sample << decBits;
  } else {
    code = pump->getBits(params->raw_bits);
    code++;
  }

  if (code < 0 || code >= params->total_values) {
    errcnt++;
  }

  if (code & 1) {
    code = -1 - code / 2;
  } else {
    code /= 2;
  }

  grads[gradient].value1 += std::abs(code);

  if (grads[gradient].value2 == params->min_value) {
    grads[gradient].value1 >>= 1;
    grads[gradient].value2 >>= 1;
  }

  grads[gradient].value2++;

  if (grad < 0) {
    interp_val = (interp_val >> 2) - code;
  } else {
    interp_val = (interp_val >> 2) + code;
  }

  if (interp_val < 0) {
    interp_val += params->total_values;
  } else if (interp_val > params->q_point[4]) {
    interp_val -= params->total_values;
  }

  if (interp_val >= 0) {
    line_buf_cur[0] = std::min(interp_val, params->q_point[4]);
  } else {
    line_buf_cur[0] = 0;
  }

  return errcnt;
}

int FujiDecompressor::fuji_decode_sample_odd(
    fuji_compressed_block* info, const fuji_compressed_params* params,
    BitPumpMSB* pump, ushort16* line_buf, int pos, int_pair* grads) {
  int interp_val = 0;
  int errcnt = 0;

  int sample = 0;
  int code = 0;
  ushort16* line_buf_cur = line_buf + pos;
  int Ra = line_buf_cur[-1];
  int Rb = line_buf_cur[-2 - params->line_width];
  int Rc = line_buf_cur[-3 - params->line_width];
  int Rd = line_buf_cur[-1 - params->line_width];
  int Rg = line_buf_cur[1];

  int grad;
  int gradient;

  grad = fuji_quant_gradient(params, Rb - Rc, Rc - Ra);
  gradient = std::abs(grad);

  if ((Rb > Rc && Rb > Rd) || (Rb < Rc && Rb < Rd)) {
    interp_val = (Rg + Ra + 2 * Rb) >> 2;
  } else {
    interp_val = (Ra + Rg) >> 1;
  }

  fuji_zerobits(pump, &sample);

  if (sample < params->max_bits - params->raw_bits - 1) {
    int decBits = bitDiff(grads[gradient].value1, grads[gradient].value2);
    code = pump->getBits(decBits);
    code += sample << decBits;
  } else {
    code = pump->getBits(params->raw_bits);
    code++;
  }

  if (code < 0 || code >= params->total_values) {
    errcnt++;
  }

  if (code & 1) {
    code = -1 - code / 2;
  } else {
    code /= 2;
  }

  grads[gradient].value1 += std::abs(code);

  if (grads[gradient].value2 == params->min_value) {
    grads[gradient].value1 >>= 1;
    grads[gradient].value2 >>= 1;
  }

  grads[gradient].value2++;

  if (grad < 0) {
    interp_val -= code;
  } else {
    interp_val += code;
  }

  if (interp_val < 0) {
    interp_val += params->total_values;
  } else if (interp_val > params->q_point[4]) {
    interp_val -= params->total_values;
  }

  if (interp_val >= 0) {
    line_buf_cur[0] = std::min(interp_val, params->q_point[4]);
  } else {
    line_buf_cur[0] = 0;
  }

  return errcnt;
}

void FujiDecompressor::fuji_decode_interpolation_even(int line_width,
                                                      ushort16* line_buf,
                                                      int pos) {
  ushort16* line_buf_cur = line_buf + pos;
  int Rb = line_buf_cur[-2 - line_width];
  int Rc = line_buf_cur[-3 - line_width];
  int Rd = line_buf_cur[-1 - line_width];
  int Rf = line_buf_cur[-4 - 2 * line_width];
  int diffRcRb = std::abs(Rc - Rb);
  int diffRfRb = std::abs(Rf - Rb);
  int diffRdRb = std::abs(Rd - Rb);

  if (diffRcRb > diffRfRb && diffRcRb > diffRdRb) {
    *line_buf_cur = (Rf + Rd + 2 * Rb) >> 2;
  } else if (diffRdRb > diffRcRb && diffRdRb > diffRfRb) {
    *line_buf_cur = (Rf + Rc + 2 * Rb) >> 2;
  } else {
    *line_buf_cur = (Rd + Rc + 2 * Rb) >> 2;
  }
}

void FujiDecompressor::fuji_extend_generic(ushort16* linebuf[_ltotal],
                                           int line_width, int start, int end) {
  for (int i = start; i <= end; i++) {
    linebuf[i][0] = linebuf[i - 1][1];
    linebuf[i][line_width + 1] = linebuf[i - 1][line_width];
  }
}

void FujiDecompressor::fuji_extend_red(ushort16* linebuf[_ltotal],
                                       int line_width) {
  fuji_extend_generic(linebuf, line_width, _R2, _R4);
}

void FujiDecompressor::fuji_extend_green(ushort16* linebuf[_ltotal],
                                         int line_width) {
  fuji_extend_generic(linebuf, line_width, _G2, _G7);
}

void FujiDecompressor::fuji_extend_blue(ushort16* linebuf[_ltotal],
                                        int line_width) {
  fuji_extend_generic(linebuf, line_width, _B2, _B4);
}

void FujiDecompressor::xtrans_decode_block( // NOLINT
    fuji_compressed_block* info, const fuji_compressed_params* params,
    BitPumpMSB* pump, int cur_line) {
  int r_even_pos = 0;
  int r_odd_pos = 1;
  int g_even_pos = 0;
  int g_odd_pos = 1;
  int b_even_pos = 0;
  int b_odd_pos = 1;

  int errcnt = 0;

  const int line_width = params->line_width;

  while (g_even_pos < line_width || g_odd_pos < line_width) {
    if (g_even_pos < line_width) {
      fuji_decode_interpolation_even(line_width, info->linebuf[_R2] + 1,
                                     r_even_pos);
      r_even_pos += 2;
      errcnt +=
          fuji_decode_sample_even(info, params, pump, info->linebuf[_G2] + 1,
                                  g_even_pos, info->grad_even[0]);
      g_even_pos += 2;
    }

    if (g_even_pos > 8) {
      errcnt +=
          fuji_decode_sample_odd(info, params, pump, info->linebuf[_R2] + 1,
                                 r_odd_pos, info->grad_odd[0]);
      r_odd_pos += 2;
      errcnt +=
          fuji_decode_sample_odd(info, params, pump, info->linebuf[_G2] + 1,
                                 g_odd_pos, info->grad_odd[0]);
      g_odd_pos += 2;
    }
  }

  fuji_extend_red(info->linebuf, line_width);
  fuji_extend_green(info->linebuf, line_width);

  g_even_pos = 0;
  g_odd_pos = 1;

  while (g_even_pos < line_width || g_odd_pos < line_width) {
    if (g_even_pos < line_width) {
      errcnt +=
          fuji_decode_sample_even(info, params, pump, info->linebuf[_G3] + 1,
                                  g_even_pos, info->grad_even[1]);
      g_even_pos += 2;
      fuji_decode_interpolation_even(line_width, info->linebuf[_B2] + 1,
                                     b_even_pos);
      b_even_pos += 2;
    }

    if (g_even_pos > 8) {
      errcnt +=
          fuji_decode_sample_odd(info, params, pump, info->linebuf[_G3] + 1,
                                 g_odd_pos, info->grad_odd[1]);
      g_odd_pos += 2;
      errcnt +=
          fuji_decode_sample_odd(info, params, pump, info->linebuf[_B2] + 1,
                                 b_odd_pos, info->grad_odd[1]);
      b_odd_pos += 2;
    }
  }

  fuji_extend_green(info->linebuf, line_width);
  fuji_extend_blue(info->linebuf, line_width);

  r_even_pos = 0;
  r_odd_pos = 1;
  g_even_pos = 0;
  g_odd_pos = 1;

  while (g_even_pos < line_width || g_odd_pos < line_width) {
    if (g_even_pos < line_width) {
      if (r_even_pos & 3) {
        errcnt +=
            fuji_decode_sample_even(info, params, pump, info->linebuf[_R3] + 1,
                                    r_even_pos, info->grad_even[2]);
      } else {
        fuji_decode_interpolation_even(line_width, info->linebuf[_R3] + 1,
                                       r_even_pos);
      }

      r_even_pos += 2;
      fuji_decode_interpolation_even(line_width, info->linebuf[_G4] + 1,
                                     g_even_pos);
      g_even_pos += 2;
    }

    if (g_even_pos > 8) {
      errcnt +=
          fuji_decode_sample_odd(info, params, pump, info->linebuf[_R3] + 1,
                                 r_odd_pos, info->grad_odd[2]);
      r_odd_pos += 2;
      errcnt +=
          fuji_decode_sample_odd(info, params, pump, info->linebuf[_G4] + 1,
                                 g_odd_pos, info->grad_odd[2]);
      g_odd_pos += 2;
    }
  }

  fuji_extend_red(info->linebuf, line_width);
  fuji_extend_green(info->linebuf, line_width);

  g_even_pos = 0;
  g_odd_pos = 1;
  b_even_pos = 0;
  b_odd_pos = 1;

  while (g_even_pos < line_width || g_odd_pos < line_width) {
    if (g_even_pos < line_width) {
      errcnt +=
          fuji_decode_sample_even(info, params, pump, info->linebuf[_G5] + 1,
                                  g_even_pos, info->grad_even[0]);
      g_even_pos += 2;

      if ((b_even_pos & 3) == 2) {
        fuji_decode_interpolation_even(line_width, info->linebuf[_B3] + 1,
                                       b_even_pos);
      } else {
        errcnt +=
            fuji_decode_sample_even(info, params, pump, info->linebuf[_B3] + 1,
                                    b_even_pos, info->grad_even[0]);
      }

      b_even_pos += 2;
    }

    if (g_even_pos > 8) {
      errcnt +=
          fuji_decode_sample_odd(info, params, pump, info->linebuf[_G5] + 1,
                                 g_odd_pos, info->grad_odd[0]);
      g_odd_pos += 2;
      errcnt +=
          fuji_decode_sample_odd(info, params, pump, info->linebuf[_B3] + 1,
                                 b_odd_pos, info->grad_odd[0]);
      b_odd_pos += 2;
    }
  }

  fuji_extend_green(info->linebuf, line_width);
  fuji_extend_blue(info->linebuf, line_width);

  r_even_pos = 0;
  r_odd_pos = 1;
  g_even_pos = 0;
  g_odd_pos = 1;

  while (g_even_pos < line_width || g_odd_pos < line_width) {
    if (g_even_pos < line_width) {
      if ((r_even_pos & 3) == 2) {
        fuji_decode_interpolation_even(line_width, info->linebuf[_R4] + 1,
                                       r_even_pos);
      } else {
        errcnt +=
            fuji_decode_sample_even(info, params, pump, info->linebuf[_R4] + 1,
                                    r_even_pos, info->grad_even[1]);
      }

      r_even_pos += 2;
      errcnt +=
          fuji_decode_sample_even(info, params, pump, info->linebuf[_G6] + 1,
                                  g_even_pos, info->grad_even[1]);
      g_even_pos += 2;
    }

    if (g_even_pos > 8) {
      errcnt +=
          fuji_decode_sample_odd(info, params, pump, info->linebuf[_R4] + 1,
                                 r_odd_pos, info->grad_odd[1]);
      r_odd_pos += 2;
      errcnt +=
          fuji_decode_sample_odd(info, params, pump, info->linebuf[_G6] + 1,
                                 g_odd_pos, info->grad_odd[1]);
      g_odd_pos += 2;
    }
  }

  fuji_extend_red(info->linebuf, line_width);
  fuji_extend_green(info->linebuf, line_width);

  g_even_pos = 0;
  g_odd_pos = 1;
  b_even_pos = 0;
  b_odd_pos = 1;

  while (g_even_pos < line_width || g_odd_pos < line_width) {
    if (g_even_pos < line_width) {
      fuji_decode_interpolation_even(line_width, info->linebuf[_G7] + 1,
                                     g_even_pos);
      g_even_pos += 2;

      if (b_even_pos & 3) {
        errcnt +=
            fuji_decode_sample_even(info, params, pump, info->linebuf[_B4] + 1,
                                    b_even_pos, info->grad_even[2]);
      } else {
        fuji_decode_interpolation_even(line_width, info->linebuf[_B4] + 1,
                                       b_even_pos);
      }

      b_even_pos += 2;
    }

    if (g_even_pos > 8) {
      errcnt +=
          fuji_decode_sample_odd(info, params, pump, info->linebuf[_G7] + 1,
                                 g_odd_pos, info->grad_odd[2]);
      g_odd_pos += 2;
      errcnt +=
          fuji_decode_sample_odd(info, params, pump, info->linebuf[_B4] + 1,
                                 b_odd_pos, info->grad_odd[2]);
      b_odd_pos += 2;
    }
  }

  fuji_extend_green(info->linebuf, line_width);
  fuji_extend_blue(info->linebuf, line_width);

  if (errcnt) {
    ThrowRDE("xtrans_decode_block");
  }
}

void FujiDecompressor::fuji_bayer_decode_block(
    fuji_compressed_block* info, const fuji_compressed_params* params,
    BitPumpMSB* pump, int cur_line) {
  int r_even_pos = 0;
  int r_odd_pos = 1;
  int g_even_pos = 0;
  int g_odd_pos = 1;
  int b_even_pos = 0;
  int b_odd_pos = 1;

  int errcnt = 0;

  const int line_width = params->line_width;

  auto pass = [&](_xt_lines c0, int g0, _xt_lines c1, int g1, int& c0_even_pos,
                  int& c1_even_pos, int& c0_odd_pos, int& c1_odd_pos) {
    while (g_even_pos < line_width || g_odd_pos < line_width) {
      if (g_even_pos < line_width) {
        errcnt +=
            fuji_decode_sample_even(info, params, pump, info->linebuf[c0] + 1,
                                    c0_even_pos, info->grad_even[g0]);
        c0_even_pos += 2;
        errcnt +=
            fuji_decode_sample_even(info, params, pump, info->linebuf[c1] + 1,
                                    c1_even_pos, info->grad_even[g0]);
        c1_even_pos += 2;
      }

      if (g_even_pos > 8) {
        errcnt +=
            fuji_decode_sample_odd(info, params, pump, info->linebuf[c0] + 1,
                                   c0_odd_pos, info->grad_odd[g1]);
        c0_odd_pos += 2;
        errcnt +=
            fuji_decode_sample_odd(info, params, pump, info->linebuf[c1] + 1,
                                   c1_odd_pos, info->grad_odd[g1]);
        c1_odd_pos += 2;
      }
    }
  };

  auto pass_RG = [&](_xt_lines c0, int g0, _xt_lines c1, int g1) {
    pass(c0, g0, c1, g1, r_even_pos, g_even_pos, r_odd_pos, g_odd_pos);

    fuji_extend_red(info->linebuf, line_width);
    fuji_extend_green(info->linebuf, line_width);
  };

  auto pass_GB = [&](_xt_lines c0, int g0, _xt_lines c1, int g1) {
    pass(c0, g0, c1, g1, g_even_pos, b_even_pos, g_odd_pos, b_odd_pos);

    fuji_extend_green(info->linebuf, line_width);
    fuji_extend_blue(info->linebuf, line_width);
  };

  pass_RG(_R2, 0, _G2, 0);

  g_even_pos = 0;
  g_odd_pos = 1;

  pass_GB(_G3, 1, _B2, 1);

  r_even_pos = 0;
  r_odd_pos = 1;
  g_even_pos = 0;
  g_odd_pos = 1;

  pass_RG(_R3, 2, _G4, 2);

  g_even_pos = 0;
  g_odd_pos = 1;
  b_even_pos = 0;
  b_odd_pos = 1;

  pass_GB(_G5, 0, _B3, 0);

  r_even_pos = 0;
  r_odd_pos = 1;
  g_even_pos = 0;
  g_odd_pos = 1;

  pass_RG(_R4, 1, _G6, 1);

  g_even_pos = 0;
  g_odd_pos = 1;
  b_even_pos = 0;
  b_odd_pos = 1;

  pass_GB(_G7, 2, _B4, 2);

  if (errcnt) {
    ThrowRDE("fuji decode bayer block");
  }
}

void FujiDecompressor::fuji_decode_strip(
    const fuji_compressed_params* info_common,
    fuji_compressed_block* info_block, const ByteStream& bs, int cur_block) {
  BitPumpMSB pump(bs);

  int cur_block_width;
  int cur_line;
  unsigned line_size;

  line_size = sizeof(ushort16) * (info_common->line_width + 2);

  cur_block_width = header.block_size;

  if (cur_block + 1 == header.blocks_in_row) {
    cur_block_width = header.raw_width % header.block_size;
  }

  struct i_pair {
    int a;
    int b;
  };

  const i_pair mtable[6] = {{_R0, _R3}, {_R1, _R4}, {_G0, _G6},
                            {_G1, _G7}, {_B0, _B3}, {_B1, _B4}};
  const i_pair ztable[3] = {{_R2, 3}, {_G2, 6}, {_B2, 3}};

  for (cur_line = 0; cur_line < header.total_lines; cur_line++) {
    if (header.raw_type == 16) {
      xtrans_decode_block(info_block, info_common, &pump, cur_line);
    } else {
      fuji_bayer_decode_block(info_block, info_common, &pump, cur_line);
    }

    // copy data from line buffers and advance
    for (auto i : mtable) {
      memcpy(info_block->linebuf[i.a], info_block->linebuf[i.b], line_size);
    }

    if (header.raw_type == 16) {
      copy_line_to_xtrans(info_block, cur_line, cur_block, cur_block_width);
    } else {
      copy_line_to_bayer(info_block, cur_line, cur_block, cur_block_width);
    }

    for (auto i : ztable) {
      memset(info_block->linebuf[i.a], 0, i.b * line_size);
      info_block->linebuf[i.a][0] = info_block->linebuf[i.a - 1][1];
      info_block->linebuf[i.a][info_common->line_width + 1] =
          info_block->linebuf[i.a - 1][info_common->line_width];
    }
  }
}

void FujiDecompressor::fuji_compressed_load_raw() {
  fuji_compressed_params common_info(*this);

  // read block sizes
  std::vector<uint32> block_sizes;
  block_sizes.resize(header.blocks_in_row);
  for (auto& block_size : block_sizes)
    block_size = input.getU32();

  // some padding?
  const uint64 raw_offset = sizeof(uint32) * header.blocks_in_row;
  if (raw_offset & 0xC) {
    const int padding = 0x10 - (raw_offset & 0xC);
    input.skipBytes(padding);
  }

  // calculating raw block offsets
  std::vector<ByteStream> strips;
  strips.reserve(header.blocks_in_row);
  for (const auto& block_size : block_sizes)
    strips.emplace_back(input.getStream(block_size));

  fuji_decode_loop(&common_info, strips);
}

void FujiDecompressor::fuji_decode_loop(
    const fuji_compressed_params* common_info, std::vector<ByteStream> strips) {

  // FIXME: figure out a sane way to pthread-ize this !

  int block = 0;
  fuji_compressed_block block_info;

  for (auto& strip : strips) {
    block_info.reset(common_info);
    fuji_decode_strip(common_info, &block_info, strip, block);
    block++;
  }
}

FujiDecompressor::FujiHeader::FujiHeader(ByteStream* bs) {
  signature = bs->getU16();
  version = bs->getByte();
  raw_type = bs->getByte();
  raw_bits = bs->getByte();
  raw_height = bs->getU16();
  raw_rounded_width = bs->getU16();
  raw_width = bs->getU16();
  block_size = bs->getU16();
  blocks_in_row = bs->getByte();
  total_lines = bs->getU16();
}

FujiDecompressor::FujiHeader::operator bool() const {
  // general validation
  const bool invalid =
      (signature != 0x4953 || version != 1 || raw_height > 0x3000 ||
       raw_height < 6 || raw_height % 6 || raw_width > 0x3000 ||
       raw_width < 0x300 || raw_width % 24 || raw_rounded_width > 0x3000 ||
       block_size < 1 || raw_rounded_width < block_size ||
       raw_rounded_width % block_size ||
       raw_rounded_width - raw_width >= block_size || block_size != 0x300 ||
       blocks_in_row > 0x10 || blocks_in_row == 0 ||
       blocks_in_row != raw_rounded_width / block_size || total_lines > 0x800 ||
       total_lines == 0 || total_lines != raw_height / 6 ||
       (raw_bits != 12 && raw_bits != 14) || (raw_type != 16 && raw_type != 0));

  return !invalid;
}

} // namespace rawspeed
