/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2016 Alexey Danilchenko
    Copyright (C) 2016 Alex Tutubalin
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

#include "decompressors/FujiDecompressor.h"
#include "common/Common.h"                                  // for roundUpDiv...
#include "common/RawImage.h"                                // for RawImage
#include "decoders/RawDecoderException.h"                   // for ThrowRDE
#include "decompressors/AbstractParallelizedDecompressor.h" // for RawDecom...
#include "io/Endianness.h"                                  // for Endianness
#include "metadata/ColorFilterArray.h"                      // for CFAColor...
#include <algorithm>                                        // for fill, min
#include <cstdlib>                                          // for abs
#include <cstring>                                          // for memcpy
// IWYU pragma: no_include <bits/std_abs.h>

namespace rawspeed {

FujiDecompressor::FujiDecompressor(const RawImage& img, ByteStream input_)
    : AbstractParallelizedDecompressor(img), input(std::move(input_)) {
  input.setByteOrder(Endianness::big);

  header = FujiHeader(&input);
  if (!header)
    ThrowRDE("compressed RAF header check");

  if (mRaw->dim != iPoint2D(header.raw_width, header.raw_height))
    ThrowRDE("RAF header specifies different dimensions!");

  if (12 == header.raw_bits) {
    ThrowRDE("Aha, finally, a 12-bit compressed RAF! Please consider providing "
             "samples on <https://raw.pixls.us/>, thanks!");
  }

  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 6; j++)
      CFA[i][j] = mRaw->cfa.getColorAt(j, i);
  }

  fuji_compressed_load_raw();
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

    /* kept for future, once there is a sample.
     total_values = 4096;
     raw_bits = 12;
     max_bits = 48;
     maxDiff = 64;
    */
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
void FujiDecompressor::copy_line(fuji_compressed_block* info,
                                 const FujiStrip& strip, int cur_line,
                                 T&& idx) const {
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

  for (int row_count = 0; row_count < FujiStrip::lineHeight(); row_count++) {
    auto* const raw_block_data = reinterpret_cast<ushort16*>(
        mRaw->getData(strip.offsetX(), strip.offsetY(cur_line) + row_count));

    for (int pixel_count = 0; pixel_count < strip.width(); pixel_count++) {
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
    }
  }
}

void FujiDecompressor::copy_line_to_xtrans(fuji_compressed_block* info,
                                           const FujiStrip& strip,
                                           int cur_line) const {
  auto index = [](int pixel_count) {
    return (((pixel_count * 2 / 3) & 0x7FFFFFFE) | ((pixel_count % 3) & 1)) +
           ((pixel_count % 3) >> 1);
  };

  copy_line(info, strip, cur_line, index);
}

void FujiDecompressor::copy_line_to_bayer(fuji_compressed_block* info,
                                          const FujiStrip& strip,
                                          int cur_line) const {
  auto index = [](int pixel_count) { return pixel_count >> 1; };

  copy_line(info, strip, cur_line, index);
}

void FujiDecompressor::fuji_zerobits(BitPumpMSB* pump, int* count) const {
  uchar8 zero = 0;
  *count = 0;

  while (zero == 0) {
    zero = pump->getBits(1);

    if (zero)
      break;

    ++*count;
  }
}

int __attribute__((const))
FujiDecompressor::bitDiff(int value1, int value2) const {
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

template <typename T1, typename T2>
int FujiDecompressor::fuji_decode_sample(T1&& func_0, T2&& func_1,
                                         fuji_compressed_block* info,
                                         BitPumpMSB* pump, ushort16* line_buf,
                                         int* pos, int_pair* grads) const {
  int interp_val = 0;
  int errcnt = 0;

  int sample = 0;
  int code = 0;
  ushort16* line_buf_cur = line_buf + *pos;

  int grad;
  int gradient;

  func_0(line_buf_cur, &interp_val, &grad, &gradient);

  fuji_zerobits(pump, &sample);

  if (sample < common_info.max_bits - common_info.raw_bits - 1) {
    int decBits = bitDiff(grads[gradient].value1, grads[gradient].value2);
    code = pump->getBits(decBits);
    code += sample << decBits;
  } else {
    code = pump->getBits(common_info.raw_bits);
    code++;
  }

  if (code < 0 || code >= common_info.total_values) {
    errcnt++;
  }

  if (code & 1) {
    code = -1 - code / 2;
  } else {
    code /= 2;
  }

  grads[gradient].value1 += std::abs(code);

  if (grads[gradient].value2 == common_info.min_value) {
    grads[gradient].value1 >>= 1;
    grads[gradient].value2 >>= 1;
  }

  grads[gradient].value2++;

  interp_val = func_1(grad, interp_val, code);

  if (interp_val < 0) {
    interp_val += common_info.total_values;
  } else if (interp_val > common_info.q_point[4]) {
    interp_val -= common_info.total_values;
  }

  if (interp_val >= 0) {
    line_buf_cur[0] = std::min(interp_val, common_info.q_point[4]);
  } else {
    line_buf_cur[0] = 0;
  }

  *pos += 2;

  return errcnt;
}

#define fuji_quant_gradient(v1, v2)                                            \
  (9 * ci.q_table[ci.q_point[4] + (v1)] + ci.q_table[ci.q_point[4] + (v2)])

int FujiDecompressor::fuji_decode_sample_even(fuji_compressed_block* info,
                                              BitPumpMSB* pump,
                                              ushort16* line_buf, int* pos,
                                              int_pair* grads) const {
  const auto& ci = common_info;
  return fuji_decode_sample(
      [&ci](const ushort16* line_buf_cur, int* interp_val, int* grad,
            int* gradient) {
        int Rb = line_buf_cur[-2 - ci.line_width];
        int Rc = line_buf_cur[-3 - ci.line_width];
        int Rd = line_buf_cur[-1 - ci.line_width];
        int Rf = line_buf_cur[-4 - 2 * ci.line_width];

        int diffRcRb;
        int diffRfRb;
        int diffRdRb;

        *grad = fuji_quant_gradient(Rb - Rf, Rc - Rb);
        *gradient = std::abs(*grad);
        diffRcRb = std::abs(Rc - Rb);
        diffRfRb = std::abs(Rf - Rb);
        diffRdRb = std::abs(Rd - Rb);

        if (diffRcRb > diffRfRb && diffRcRb > diffRdRb) {
          *interp_val = Rf + Rd + 2 * Rb;
        } else if (diffRdRb > diffRcRb && diffRdRb > diffRfRb) {
          *interp_val = Rf + Rc + 2 * Rb;
        } else {
          *interp_val = Rd + Rc + 2 * Rb;
        }
      },
      [](int grad, int interp_val, int code) {
        if (grad < 0) {
          interp_val = (interp_val >> 2) - code;
        } else {
          interp_val = (interp_val >> 2) + code;
        }

        return interp_val;
      },
      info, pump, line_buf, pos, grads);
}

int FujiDecompressor::fuji_decode_sample_odd(fuji_compressed_block* info,
                                             BitPumpMSB* pump,
                                             ushort16* line_buf, int* pos,
                                             int_pair* grads) const {
  const auto& ci = common_info;
  return fuji_decode_sample(
      [&ci](const ushort16* line_buf_cur, int* interp_val, int* grad,
            int* gradient) {
        int Ra = line_buf_cur[-1];
        int Rb = line_buf_cur[-2 - ci.line_width];
        int Rc = line_buf_cur[-3 - ci.line_width];
        int Rd = line_buf_cur[-1 - ci.line_width];
        int Rg = line_buf_cur[1];

        *grad = fuji_quant_gradient(Rb - Rc, Rc - Ra);
        *gradient = std::abs(*grad);

        if ((Rb > Rc && Rb > Rd) || (Rb < Rc && Rb < Rd)) {
          *interp_val = (Rg + Ra + 2 * Rb) >> 2;
        } else {
          *interp_val = (Ra + Rg) >> 1;
        }
      },
      [](int grad, int interp_val, int code) {
        if (grad < 0) {
          interp_val -= code;
        } else {
          interp_val += code;
        }

        return interp_val;
      },
      info, pump, line_buf, pos, grads);
}

#undef fuji_quant_gradient

void FujiDecompressor::fuji_decode_interpolation_even(int line_width,
                                                      ushort16* line_buf,
                                                      int* pos) const {
  ushort16* line_buf_cur = line_buf + *pos;
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

  *pos += 2;
}

void FujiDecompressor::fuji_extend_generic(ushort16* linebuf[_ltotal],
                                           int line_width, int start,
                                           int end) const {
  for (int i = start; i <= end; i++) {
    linebuf[i][0] = linebuf[i - 1][1];
    linebuf[i][line_width + 1] = linebuf[i - 1][line_width];
  }
}

void FujiDecompressor::fuji_extend_red(ushort16* linebuf[_ltotal],
                                       int line_width) const {
  fuji_extend_generic(linebuf, line_width, _R2, _R4);
}

void FujiDecompressor::fuji_extend_green(ushort16* linebuf[_ltotal],
                                         int line_width) const {
  fuji_extend_generic(linebuf, line_width, _G2, _G7);
}

void FujiDecompressor::fuji_extend_blue(ushort16* linebuf[_ltotal],
                                        int line_width) const {
  fuji_extend_generic(linebuf, line_width, _B2, _B4);
}

void FujiDecompressor::xtrans_decode_block(fuji_compressed_block* info,
                                           BitPumpMSB* pump,
                                           int cur_line) const {
  struct ColorPos {
    int even = 0;
    int odd = 1;

    void reset() {
      even = 0;
      odd = 1;
    }
  };

  ColorPos r;
  ColorPos g;
  ColorPos b;

  int errcnt = 0;

  const int line_width = common_info.line_width;

  // FIXME: GCC5 sucks.
  // https://github.com/darktable-org/rawspeed/issues/112
  // https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=871250
  // https://bugs.launchpad.net/linuxmint/+bug/1709234
  auto pass = [&, line_width](auto&& even_func, _xt_lines c0, _xt_lines c1,
                              int grad, ColorPos& c0_pos, ColorPos& c1_pos) {
    while (g.even < line_width || g.odd < line_width) {
      if (g.even < line_width)
        even_func(c0, c1, grad, c0_pos, c1_pos);

      if (g.even > 8) {
        errcnt += fuji_decode_sample_odd(info, pump, info->linebuf[c0] + 1,
                                         &c0_pos.odd, info->grad_odd[grad]);
        errcnt += fuji_decode_sample_odd(info, pump, info->linebuf[c1] + 1,
                                         &c1_pos.odd, info->grad_odd[grad]);
      }
    }
  };

  pass(
      [&](_xt_lines c0, _xt_lines c1, int grad, ColorPos& c0_pos,
          ColorPos& c1_pos) {
        fuji_decode_interpolation_even(line_width, info->linebuf[c0] + 1,
                                       &c0_pos.even);
        errcnt += fuji_decode_sample_even(info, pump, info->linebuf[c1] + 1,
                                          &c1_pos.even, info->grad_even[grad]);
      },
      _R2, _G2, 0, r, g);

  fuji_extend_red(info->linebuf, line_width);
  fuji_extend_green(info->linebuf, line_width);

  g.reset();

  pass(
      [&](_xt_lines c0, _xt_lines c1, int grad, ColorPos& c0_pos,
          ColorPos& c1_pos) {
        errcnt += fuji_decode_sample_even(info, pump, info->linebuf[c0] + 1,
                                          &c0_pos.even, info->grad_even[grad]);
        fuji_decode_interpolation_even(line_width, info->linebuf[c1] + 1,
                                       &c1_pos.even);
      },
      _G3, _B2, 1, g, b);

  fuji_extend_green(info->linebuf, line_width);
  fuji_extend_blue(info->linebuf, line_width);

  r.reset();
  g.reset();

  pass(
      [&](_xt_lines c0, _xt_lines c1, int grad, ColorPos& c0_pos,
          ColorPos& c1_pos) {
        if (c0_pos.even & 3) {
          errcnt +=
              fuji_decode_sample_even(info, pump, info->linebuf[c0] + 1,
                                      &c0_pos.even, info->grad_even[grad]);
        } else {
          fuji_decode_interpolation_even(line_width, info->linebuf[c0] + 1,
                                         &c0_pos.even);
        }

        fuji_decode_interpolation_even(line_width, info->linebuf[c1] + 1,
                                       &c1_pos.even);
      },
      _R3, _G4, 2, r, g);

  fuji_extend_red(info->linebuf, line_width);
  fuji_extend_green(info->linebuf, line_width);

  g.reset();
  b.reset();

  pass(
      [&](_xt_lines c0, _xt_lines c1, int grad, ColorPos& c0_pos,
          ColorPos& c1_pos) {
        errcnt += fuji_decode_sample_even(info, pump, info->linebuf[c0] + 1,
                                          &c0_pos.even, info->grad_even[grad]);

        if ((c1_pos.even & 3) == 2) {
          fuji_decode_interpolation_even(line_width, info->linebuf[c1] + 1,
                                         &c1_pos.even);
        } else {
          errcnt +=
              fuji_decode_sample_even(info, pump, info->linebuf[c1] + 1,
                                      &c1_pos.even, info->grad_even[grad]);
        }
      },
      _G5, _B3, 0, g, b);

  fuji_extend_green(info->linebuf, line_width);
  fuji_extend_blue(info->linebuf, line_width);

  r.reset();
  g.reset();

  pass(
      [&](_xt_lines c0, _xt_lines c1, int grad, ColorPos& c0_pos,
          ColorPos& c1_pos) {
        if ((c0_pos.even & 3) == 2) {
          fuji_decode_interpolation_even(line_width, info->linebuf[c0] + 1,
                                         &c0_pos.even);
        } else {
          errcnt +=
              fuji_decode_sample_even(info, pump, info->linebuf[c0] + 1,
                                      &c0_pos.even, info->grad_even[grad]);
        }

        errcnt += fuji_decode_sample_even(info, pump, info->linebuf[c1] + 1,
                                          &c1_pos.even, info->grad_even[grad]);
      },
      _R4, _G6, 1, r, g);

  fuji_extend_red(info->linebuf, line_width);
  fuji_extend_green(info->linebuf, line_width);

  g.reset();
  b.reset();

  pass(
      [&](_xt_lines c0, _xt_lines c1, int grad, ColorPos& c0_pos,
          ColorPos& c1_pos) {
        fuji_decode_interpolation_even(line_width, info->linebuf[c0] + 1,
                                       &c0_pos.even);

        if (c1_pos.even & 3) {
          errcnt +=
              fuji_decode_sample_even(info, pump, info->linebuf[c1] + 1,
                                      &c1_pos.even, info->grad_even[grad]);
        } else {
          fuji_decode_interpolation_even(line_width, info->linebuf[c1] + 1,
                                         &c1_pos.even);
        }
      },
      _G7, _B4, 2, g, b);

  fuji_extend_green(info->linebuf, line_width);
  fuji_extend_blue(info->linebuf, line_width);

  if (errcnt)
    ThrowRDE("xtrans_decode_block");
}

void FujiDecompressor::fuji_bayer_decode_block(fuji_compressed_block* info,
                                               BitPumpMSB* pump,
                                               int cur_line) const {
  struct ColorPos {
    int even = 0;
    int odd = 1;

    void reset() {
      even = 0;
      odd = 1;
    }
  };

  ColorPos r;
  ColorPos g;
  ColorPos b;

  int errcnt = 0;

  const int line_width = common_info.line_width;

  auto pass = [&](_xt_lines c0, _xt_lines c1, int grad, ColorPos& c0_pos,
                  ColorPos& c1_pos) {
    while (g.even < line_width || g.odd < line_width) {
      if (g.even < line_width) {
        errcnt += fuji_decode_sample_even(info, pump, info->linebuf[c0] + 1,
                                          &c0_pos.even, info->grad_even[grad]);
        errcnt += fuji_decode_sample_even(info, pump, info->linebuf[c1] + 1,
                                          &c1_pos.even, info->grad_even[grad]);
      }

      if (g.even > 8) {
        errcnt += fuji_decode_sample_odd(info, pump, info->linebuf[c0] + 1,
                                         &c0_pos.odd, info->grad_odd[grad]);
        errcnt += fuji_decode_sample_odd(info, pump, info->linebuf[c1] + 1,
                                         &c1_pos.odd, info->grad_odd[grad]);
      }
    }
  };

  auto pass_RG = [&](_xt_lines c0, _xt_lines c1, int grad) {
    pass(c0, c1, grad, r, g);

    fuji_extend_red(info->linebuf, line_width);
    fuji_extend_green(info->linebuf, line_width);
  };

  auto pass_GB = [&](_xt_lines c0, _xt_lines c1, int grad) {
    pass(c0, c1, grad, g, b);

    fuji_extend_green(info->linebuf, line_width);
    fuji_extend_blue(info->linebuf, line_width);
  };

  pass_RG(_R2, _G2, 0);

  g.reset();

  pass_GB(_G3, _B2, 1);

  r.reset();
  g.reset();

  pass_RG(_R3, _G4, 2);

  g.reset();
  b.reset();

  pass_GB(_G5, _B3, 0);

  r.reset();
  g.reset();

  pass_RG(_R4, _G6, 1);

  g.reset();
  b.reset();

  pass_GB(_G7, _B4, 2);

  if (errcnt)
    ThrowRDE("fuji decode bayer block");
}

void FujiDecompressor::fuji_decode_strip(
    fuji_compressed_block* info_block, const FujiStrip& strip) const {
  BitPumpMSB pump(strip.bs);

  const unsigned line_size = sizeof(ushort16) * (common_info.line_width + 2);

  struct i_pair {
    int a;
    int b;
  };

  const i_pair mtable[6] = {{_R0, _R3}, {_R1, _R4}, {_G0, _G6},
                            {_G1, _G7}, {_B0, _B3}, {_B1, _B4}};
  const i_pair ztable[3] = {{_R2, 3}, {_G2, 6}, {_B2, 3}};

  for (int cur_line = 0; cur_line < strip.height(); cur_line++) {
    if (header.raw_type == 16) {
      xtrans_decode_block(info_block, &pump, cur_line);
    } else {
      fuji_bayer_decode_block(info_block, &pump, cur_line);
    }

    // copy data from line buffers and advance
    for (auto i : mtable) {
      memcpy(info_block->linebuf[i.a], info_block->linebuf[i.b], line_size);
    }

    if (header.raw_type == 16) {
      copy_line_to_xtrans(info_block, strip, cur_line);
    } else {
      copy_line_to_bayer(info_block, strip, cur_line);
    }

    for (auto i : ztable) {
      memset(info_block->linebuf[i.a], 0, i.b * line_size);
      info_block->linebuf[i.a][0] = info_block->linebuf[i.a - 1][1];
      info_block->linebuf[i.a][common_info.line_width + 1] =
          info_block->linebuf[i.a - 1][common_info.line_width];
    }
  }
}

void FujiDecompressor::fuji_compressed_load_raw() {
  common_info = fuji_compressed_params(*this);

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
  strips.reserve(header.blocks_in_row);

  int block = 0;
  for (const auto& block_size : block_sizes) {
    strips.emplace_back(header, block, input.getStream(block_size));
    block++;
  }
}

void FujiDecompressor::decompress() const {
  startThreading(header.blocks_in_row);
}

void FujiDecompressor::decompressThreaded(
    const RawDecompressorThread* t) const {
  fuji_compressed_block block_info;

  for (size_t i = t->start; i < t->end && i < strips.size(); i++) {
    block_info.reset(&common_info);
    fuji_decode_strip(&block_info, strips[i]);
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
       raw_height < FujiStrip::lineHeight() ||
       raw_height % FujiStrip::lineHeight() || raw_width > 0x3000 ||
       raw_width < 0x300 || raw_width % 24 || raw_rounded_width > 0x3000 ||
       block_size != 0x300 || raw_rounded_width < block_size ||
       raw_rounded_width % block_size ||
       raw_rounded_width - raw_width >= block_size || blocks_in_row > 0x10 ||
       blocks_in_row == 0 || blocks_in_row != raw_rounded_width / block_size ||
       blocks_in_row != roundUpDivision(raw_width, block_size) ||
       total_lines > 0x800 || total_lines == 0 ||
       total_lines != raw_height / FujiStrip::lineHeight() ||
       (raw_bits != 12 && raw_bits != 14) || (raw_type != 16 && raw_type != 0));

  return !invalid;
}

} // namespace rawspeed
