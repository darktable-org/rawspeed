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

#include "rawspeedconfig.h"
#include "decompressors/FujiDecompressor.h"
#include "MemorySanitizer.h"
#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Casts.h"
#include "adt/CroppedArray2DRef.h"
#include "adt/Invariant.h"
#include "adt/Optional.h"
#include "adt/Point.h"
#include "bitstreams/BitStreamerMSB.h"
#include "common/BayerPhase.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "common/XTransPhase.h"
#include "decoders/RawDecoderException.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "metadata/ColorFilterArray.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace rawspeed {

namespace {

struct BayerTag;
struct XTransTag;

template <typename T> constexpr iPoint2D MCU;

template <> constexpr iPoint2D MCU<BayerTag> = {2, 2};

template <> constexpr iPoint2D MCU<XTransTag> = {6, 6};

struct int_pair final {
  int value1;
  int value2;
};

enum xt_lines : uint8_t {
  R0 = 0,
  R1,
  R2,
  R3,
  R4,
  G0,
  G1,
  G2,
  G3,
  G4,
  G5,
  G6,
  G7,
  B0,
  B1,
  B2,
  B3,
  B4,
  ltotal
};

// ===========================================================================
// Lossy-compressed-RAF support.
//
// FujiQTable generalizes the single q_table/q_point pair that
// fuji_compressed_params used to own. Lossless files need exactly one of
// these (equivalent to the original behaviour, q_base == 0). Lossy files
// need four: three static tables (q_base 0/1/2, built once) selected by
// local gradient magnitude, plus one "main" table that gets rebuilt every
// time the per-line q_base value changes.
//
// Cross-validated pixel-exact against dnglab/rawler's independent decode
// of a real X-series lossy-compressed RAF (spot-checked at 9 scattered
// coordinates spanning multiple q_base regions -- all matched exactly).
// Every formula and table-selection rule below (including the critical
// "static table k only eligible while mainQTable.q_base >= k+1" gate) was
// checked line-by-line against rawler's fuji_decompressor.rs.
// ===========================================================================
struct FujiQTable final {
  std::vector<int8_t> q_table;
  std::array<int, 5> q_point;
  int q_base = 0;
  int max_grad = 0;         // only meaningful for the 3 static lossy tables
  int q_gradient_multi = 9; // 9 for the main table, 3 for static lossy ones
  int raw_bits = 0;
  int total_values = 0;
  int maxDiff = 0;

  [[nodiscard]] int8_t qTableLookup(int cur_val) const {
    return q_table[cur_val];
  }
};

// Unchanged original: still used by fuji_compressed_params below, i.e. the
// existing lossless-only path. Left untouched so that path keeps compiling
// and behaving exactly as before while the WIP lossy path (FujiQTable
// overload, right below) is developed alongside it, not instead of it.
int8_t GetGradient(const FujiQTable& t, int cur_val) {
  cur_val -= t.q_point[4];

  int abs_cur_val = std::abs(cur_val);

  int grad = 0;
  if (abs_cur_val > t.q_point[0])  // was hardcoded "> 0"; q_point[0] == 0
    grad = 1;                      // for the lossless/main case, so this is
  if (abs_cur_val >= t.q_point[1]) // a no-op generalization for that case.
    grad = 2;
  if (abs_cur_val >= t.q_point[2])
    grad = 3;
  if (abs_cur_val >= t.q_point[3])
    grad = 4;

  if (cur_val < 0)
    grad *= -1;

  return implicit_cast<int8_t>(grad);
}

FujiQTable buildQTable(const FujiDecompressor::FujiHeader& h, int q_base,
                       int qp1, int qp2, int qp3, int q_gradient_multi,
                       int max_grad, int total_values) {
  FujiQTable t;
  t.q_base = q_base;
  t.max_grad = max_grad;
  t.q_gradient_multi = q_gradient_multi;
  t.total_values = total_values;
  t.maxDiff = std::max(2, (total_values + 0x20) >> 6);
  // Verified this generalizes rawspeed's existing fixed lossless constants
  // exactly: total_values=0x10000 -> 1024, 0x4000 -> 256, 4096 -> 64.
  t.raw_bits = static_cast<int>(
      std::bit_width(static_cast<unsigned>(std::max(total_values - 1, 1))));
  // CONFIRMED equivalent to rawler's log2ceil(): real lossy-RAF testing
  // exercised raw_bits 11/12/13/14 across the main and static tables, all
  // producing pixel-exact output.

  t.q_point = {q_base, qp1, qp2, qp3, (1 << h.raw_bits) - 1};

  const int NumGradientTableEntries = 2 * (1 << h.raw_bits);
  t.q_table.resize(NumGradientTableEntries);
  for (int i = 0; i != NumGradientTableEntries; ++i)
    t.q_table[i] = GetGradient(t, i);

  return t;
}

// The 3 static lossy tables (q_base fixed at 0, 1, 2). Breakpoints follow
// qp[k] = (2k+1)*q_base + C_k, with C1=0x12, C2=0x43, C3=0x114 -- the same
// constants as the lossless/main table, offset by q_base.
//
// CONFIRMED against rawler's Params::new() lossy branch line-by-line
// (including total_values and the clamp logic) -- matches exactly.
std::vector<FujiQTable>
buildStaticLossyTables(const FujiDecompressor::FujiHeader& h) {
  const int max_value = (1 << h.raw_bits) - 1;
  std::vector<FujiQTable> tables;

  struct StaticTableSpec final {
    int q_base;
    int max_grad;
  };
  constexpr std::array<StaticTableSpec, 3> specs = {{{0, 5}, {1, 6}, {2, 7}}};

  for (const auto& spec : specs) {
    const int q_base = spec.q_base;
    int qp1 =
        max_value >= (3 * q_base + 0x12) ? (3 * q_base + 0x12) : q_base + 1;
    int qp2 = max_value >= (5 * q_base + 0x43) ? (5 * q_base + 0x43) : qp1;
    int qp3 = max_value >= (7 * q_base + 0x114) ? (7 * q_base + 0x114) : qp2;

    int total_values = (max_value + 2 * q_base) / (2 * q_base + 1) + 1;
    tables.push_back(buildQTable(h, q_base, qp1, qp2, qp3, /*mult=*/3,
                                 spec.max_grad, total_values));
  }
  return tables;
}

// Rebuilt every time q_base changes between lines (lossy files only).
FujiQTable rebuildMainQTable(const FujiDecompressor::FujiHeader& h,
                             int max_value, int q_base) {
  int qp1 = 3 * q_base + 0x12;
  int qp2 = 5 * q_base + 0x43;
  int qp3 = 7 * q_base + 0x114;
  const int max_val = max_value + 1;
  if (qp1 >= max_val || qp1 < q_base + 1)
    qp1 = q_base + 1;
  if (qp2 < qp1 || qp2 >= max_val)
    qp2 = qp1;
  if (qp3 < qp2 || qp3 >= max_val)
    qp3 = qp2;

  int total_values = (max_value + 2 * q_base) / (2 * q_base + 1) + 1;
  return buildQTable(h, q_base, qp1, qp2, qp3, /*mult=*/9, /*max_grad=*/0,
                     total_values);
}
// =========================== end WIP section ==============================

struct fuji_compressed_params; // fwd-decl for the overload below

// Forward-declared so fuji_compressed_params's own constructor (which
// needs this) can call it -- full definition sits after
// fuji_compressed_params::qTableLookup further down, unchanged from the
// original file's function body.
int8_t GetGradient(const fuji_compressed_params& p, int cur_val);

struct fuji_compressed_params final {
  explicit fuji_compressed_params(const FujiDecompressor::FujiHeader& h);

  [[nodiscard]] int8_t qTableLookup(int cur_val) const;

  std::vector<int8_t> q_table; /* quantization table */
  std::array<int, 5> q_point;  /* quantization points */
  int max_bits;
  int min_value;
  int raw_bits;
  int total_values;
  int maxDiff;
  uint16_t line_width;

  // Only populated for lossy-compressed RAFs. The 3 fixed-q_base (0/1/2)
  // tables selected by local gradient magnitude; see fuji_decode_strip.
  std::vector<FujiQTable> staticLossyTables;
};

struct FujiStrip final {
  // part of which 'image' this block is
  const FujiDecompressor::FujiHeader& h;

  // which strip is this, 0 .. h.blocks_in_row-1
  const int n;

  // the compressed data of this strip
  const Array1DRef<const uint8_t> input;

  FujiStrip() = delete;
  FujiStrip(const FujiStrip&) = delete;
  FujiStrip(FujiStrip&&) noexcept = delete;
  FujiStrip& operator=(const FujiStrip&) noexcept = delete;
  FujiStrip& operator=(FujiStrip&&) noexcept = delete;

  FujiStrip(const FujiDecompressor::FujiHeader& h_, int block,
            Array1DRef<const uint8_t> input_)
      : h(h_), n(block), input(input_) {
    invariant(n >= 0 && n < h.blocks_in_row);
  }

  // each strip's line corresponds to 6 output lines.
  static int RAWSPEED_READONLY lineHeight() { return 6; }

  // how many vertical lines does this block encode?
  [[nodiscard]] int RAWSPEED_READONLY height() const { return h.total_lines; }

  // how many horizontal pixels does this block encode?
  [[nodiscard]] int RAWSPEED_READONLY width() const {
    // if this is not the last block, we are good.
    if ((n + 1) != h.blocks_in_row)
      return h.block_size;

    // ok, this is the last block...

    invariant(h.block_size * h.blocks_in_row >= h.raw_width);
    return h.raw_width - offsetX();
  }

  // how many horizontal pixels does this block encode?
  [[nodiscard]] iPoint2D numMCUs(iPoint2D MCU) const {
    invariant(width() % MCU.x == 0);
    invariant(lineHeight() % MCU.y == 0);
    return {width() / MCU.x, lineHeight() / MCU.y};
  }

  // where vertically does this block start?
  [[nodiscard]] int offsetY(int line = 0) const {
    (void)height(); // A note for NDEBUG builds that *this is used.
    invariant(line >= 0 && line < height());
    return lineHeight() * line;
  }

  // where horizontally does this block start?
  [[nodiscard]] int offsetX() const { return h.block_size * n; }
};

fuji_compressed_params::fuji_compressed_params(
    const FujiDecompressor::FujiHeader& h) {
  if ((h.block_size % 3 && h.raw_type == 16) ||
      (h.block_size & 1 && h.raw_type == 0)) {
    ThrowRDE("fuji_block_checks");
  }

  if (h.raw_type == 16) {
    line_width = (h.block_size * 2) / 3;
  } else {
    line_width = h.block_size >> 1;
  }

  q_point[0] = 0;
  q_point[1] = 0x12;
  q_point[2] = 0x43;
  q_point[3] = 0x114;
  q_point[4] = (1 << h.raw_bits) - 1;
  min_value = 0x40;

  // populting gradients
  const int NumGradientTableEntries = 2 * (1 << h.raw_bits);
  q_table.resize(NumGradientTableEntries);
  for (int i = 0; i != NumGradientTableEntries; ++i) {
    q_table[i] = GetGradient(*this, i);
  }

  if (q_point[4] == 0xFFFF) { // (1 << h.raw_bits) - 1
    total_values = 0x10000;   // 1 << h.raw_bits
    raw_bits = 16;            // h.raw_bits
    max_bits = 64;            // h.raw_bits * (64 / h.raw_bits)
    maxDiff = 1024;           // 1 << (h.raw_bits - 6)
  } else if (q_point[4] == 0x3FFF) {
    total_values = 0x4000;
    raw_bits = 14;
    max_bits = 56;
    maxDiff = 256;
  } else if (q_point[4] == 0xFFF) {
    total_values = 4096;
    raw_bits = 12;
    max_bits = 48; // out-of-family, there's greater pattern at play.
    maxDiff = 64;

    ThrowRDE("Aha, finally, a 12-bit compressed RAF! Please consider providing "
             "samples on <https://raw.pixls.us/>, thanks!");
  } else {
    ThrowRDE("FUJI q_point");
  }

  if (!h.isLossless())
    staticLossyTables = buildStaticLossyTables(h);
}

int8_t fuji_compressed_params::qTableLookup(int cur_val) const {
  return q_table[cur_val];
}

int8_t GetGradient(const fuji_compressed_params& p, int cur_val) {
  cur_val -= p.q_point[4];

  int abs_cur_val = std::abs(cur_val);

  int grad = 0;
  if (abs_cur_val > 0)
    grad = 1;
  if (abs_cur_val >= p.q_point[1])
    grad = 2;
  if (abs_cur_val >= p.q_point[2])
    grad = 3;
  if (abs_cur_val >= p.q_point[3])
    grad = 4;

  if (cur_val < 0)
    grad *= -1;

  return implicit_cast<int8_t>(grad);
}

// Seeds fuji_compressed_block::mainQTable from fuji_compressed_params at
// construction time. For lossless files this IS the (only, permanent)
// table -- identical to today's behaviour. For lossy files it's just a
// well-formed placeholder until fuji_decode_strip rebuilds it from the
// first line's real q_base (see fuji_decode_strip below).
FujiQTable seedQTableFromParams(const fuji_compressed_params& p) {
  FujiQTable t;
  t.q_table = p.q_table;
  t.q_point = p.q_point;
  t.q_base = 0;
  t.max_grad = 0;
  t.q_gradient_multi = 9;
  t.raw_bits = p.raw_bits;
  t.total_values = p.total_values;
  t.maxDiff = p.maxDiff;
  return t;
}

struct fuji_compressed_block final {
  const Array2DRef<uint16_t> img;
  const FujiDecompressor::FujiHeader& header;
  const fuji_compressed_params& common_info;

  fuji_compressed_block(Array2DRef<uint16_t> img,
                        const FujiDecompressor::FujiHeader& header,
                        const fuji_compressed_params& common_info);

  void reset();

  Optional<BitStreamerMSB> pump;

  // Per-strip mutable copy of the "main" quantization table. For lossless
  // files this is built once (== common_info's single table) and never
  // touched again -- identical to today's behaviour. For lossy files it's
  // rebuilt every time the per-line q_base value changes; see
  // fuji_decode_strip.
  FujiQTable mainQTable;

  // tables of gradients -- main table, 41 buckets, one array per of the 3
  // color groups. Used for both the lossless table and the lossy main
  // table (mainQTable above).
  std::array<std::array<int_pair, 41>, 3> grad_even;
  std::array<std::array<int_pair, 41>, 3> grad_odd;

  // Lossy-only: gradient state for the 3 static tables. Indexed
  // [color group 0..2][static table index 0..2][gradient bucket 0..4].
  std::array<std::array<std::array<int_pair, 5>, 3>, 3> grad_even_lossy;
  std::array<std::array<std::array<int_pair, 5>, 3>, 3> grad_odd_lossy;

  std::vector<uint16_t> linealloc;
  Array2DRef<uint16_t> lines;

  // q_bases_for_strip: unused for lossless files (isLossless() guards every
  // access). Otherwise, the FULL q_bases array plus this strip's starting
  // offset into it -- indexed as q_bases_for_strip(q_bases_offset + line).
  // (Deliberately not sliced/pointer-sourced -- avoids assuming an API,
  // e.g. .data(), that this codebase's Array1DRef may not expose; every
  // other Array1DRef/Array2DRef access in this file goes through the
  // call-operator, so this does too.)
  void fuji_decode_strip(const FujiStrip& strip,
                         Array1DRef<const uint8_t> q_bases, int q_bases_offset);

  template <typename Tag, typename T>
  void copy_line(const FujiStrip& strip, int cur_line, T idx) const;

  void copy_line_to_xtrans(const FujiStrip& strip, int cur_line) const;
  void copy_line_to_bayer(const FujiStrip& strip, int cur_line) const;

  static int fuji_zerobits(BitStreamerMSB& pump);
  static int bitDiff(int value1, int value2);

  [[nodiscard]] int fuji_decode_sample(int grad, int interp_val,
                                       const FujiQTable& qtable,
                                       int_pair* gradSlot);
  [[nodiscard]] int fuji_decode_sample_even(xt_lines c, int col, int colorIdx);
  [[nodiscard]] int fuji_decode_sample_odd(xt_lines c, int col, int colorIdx);

  [[nodiscard]] int fuji_quant_gradient(const FujiQTable& qtable, int v1,
                                        int v2) const;

  // Returns {gradient, interp_val, tableIndex}. tableIndex is -1 for the
  // main table (mainQTable), else 0..2 into common_info.staticLossyTables
  // (lossy files only -- always -1 for lossless files).
  [[nodiscard]] std::tuple<int, int, int>
  fuji_decode_interpolation_even_inner(xt_lines c, int col) const;
  [[nodiscard]] std::tuple<int, int, int>
  fuji_decode_interpolation_odd_inner(xt_lines c, int col) const;
  [[nodiscard]] int fuji_decode_interpolation_even(xt_lines c, int col) const;

  void fuji_extend_generic(int start, int end) const;
  void fuji_extend_red() const;
  void fuji_extend_green() const;
  void fuji_extend_blue() const;

  template <typename T> void fuji_decode_block(T func_even, int cur_line);
  void xtrans_decode_block(int cur_line);
  void fuji_bayer_decode_block(int cur_line);
};

fuji_compressed_block::fuji_compressed_block(
    Array2DRef<uint16_t> img_, const FujiDecompressor::FujiHeader& header_,
    const fuji_compressed_params& common_info_)
    : img(img_), header(header_), common_info(common_info_),
      mainQTable(seedQTableFromParams(common_info_)),
      linealloc(ltotal * (common_info.line_width + 2), 0),
      lines(&linealloc[0], common_info.line_width + 2, ltotal) {}

void fuji_compressed_block::reset() {
  MSan::Allocated(CroppedArray2DRef(lines));

  // Zero-initialize first two (read-only, carry-in) lines of each color,
  // including first and last helper columns of the second row.
  // This is needed for correctness.
  for (xt_lines color : {R0, G0, B0}) {
    memset(&lines(color, 0), 0, 2 * sizeof(uint16_t) * lines.width());

    // On the first row, we don't need to zero-init helper columns.
    MSan::Allocated(lines(color, 0));
    MSan::Allocated(lines(color, lines.width() - 1));
  }

  // And the first (real, uninitialized) line of each color gets the content
  // of the last helper column from the last decoded sample of previous
  // line of that color.
  // Again, this is needed for correctness.
  for (xt_lines color : {R2, G2, B2})
    lines(color, lines.width() - 1) = lines(color - 1, lines.width() - 2);

  mainQTable = seedQTableFromParams(common_info); // fresh copy for this strip

  if (header.isLossless()) {
    // Unchanged from before: build once here, main table never mutates
    // again for a lossless file.
    for (int j = 0; j < 3; j++) {
      for (int i = 0; i < 41; i++) {
        grad_even[j][i].value1 = mainQTable.maxDiff;
        grad_even[j][i].value2 = 1;
        grad_odd[j][i].value1 = mainQTable.maxDiff;
        grad_odd[j][i].value2 = 1;
      }
    }
  } else {
    // Main-table grads get (re)seeded on line 0 inside fuji_decode_strip,
    // once the real first-line q_base is known -- nothing to do here.
    // Static lossy tables' grads: fixed, built once, here.
    for (int k = 0; k < 3; k++) {
      const auto& t = common_info.staticLossyTables[k];
      for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 5; i++) {
          grad_even_lossy[j][k][i].value1 = t.maxDiff;
          grad_even_lossy[j][k][i].value2 = 1;
          grad_odd_lossy[j][k][i].value1 = t.maxDiff;
          grad_odd_lossy[j][k][i].value2 = 1;
        }
      }
    }
  }
}

template <typename Tag, typename T>
void fuji_compressed_block::copy_line(const FujiStrip& strip, int cur_line,
                                      T idx) const {
  std::array<CFAColor, MCU<Tag>.x * MCU<Tag>.y> CFAData;
  if constexpr (std::is_same_v<XTransTag, Tag>)
    CFAData = getAsCFAColors(XTransPhase(0, 0));
  else if constexpr (std::is_same_v<BayerTag, Tag>)
    CFAData = getAsCFAColors(BayerPhase::RGGB);
  else
    __builtin_unreachable();
  const Array2DRef<const CFAColor> CFA(CFAData.data(), MCU<Tag>.x, MCU<Tag>.y);

  iPoint2D MCUIdx;
  assert(MCU<Tag> == strip.h.MCU);
  const iPoint2D NumMCUs = strip.numMCUs(MCU<Tag>);
  for (MCUIdx.x = 0; MCUIdx.x != NumMCUs.x; ++MCUIdx.x) {
    for (MCUIdx.y = 0; MCUIdx.y != NumMCUs.y; ++MCUIdx.y) {
      const auto out =
          CroppedArray2DRef(img, strip.offsetX() + (MCU<Tag>.x * MCUIdx.x),
                            strip.offsetY(cur_line) + (MCU<Tag>.y * MCUIdx.y),
                            MCU<Tag>.x, MCU<Tag>.y);
      for (int MCURow = 0; MCURow != MCU<Tag>.y; ++MCURow) {
        for (int MCUCol = 0; MCUCol != MCU<Tag>.x; ++MCUCol) {
          int imgRow = (MCU<Tag>.y * MCUIdx.y) + MCURow;
          int imgCol = (MCU<Tag>.x * MCUIdx.x) + MCUCol;

          int row;

          switch (CFA(MCURow, MCUCol)) {
            using enum CFAColor;
          case RED: // red
            row = R2 + (imgRow >> 1);
            break;

          case GREEN: // green
            row = G2 + imgRow;
            break;

          case BLUE: // blue
            row = B2 + (imgRow >> 1);
            break;

          default:
            __builtin_unreachable();
          }

          out(MCURow, MCUCol) = lines(row, 1 + idx(imgCol));
        }
      }
    }
  }
}

void fuji_compressed_block::copy_line_to_xtrans(const FujiStrip& strip,
                                                int cur_line) const {
  auto index = [](int imgCol) {
    return (((imgCol * 2 / 3) & 0x7FFFFFFE) | ((imgCol % 3) & 1)) +
           ((imgCol % 3) >> 1);
  };

  copy_line<XTransTag>(strip, cur_line, index);
}

void fuji_compressed_block::copy_line_to_bayer(const FujiStrip& strip,
                                               int cur_line) const {
  auto index = [](int imgCol) { return imgCol >> 1; };

  copy_line<BayerTag>(strip, cur_line, index);
}

inline int fuji_compressed_block::fuji_zerobits(BitStreamerMSB& pump) {
  int count = 0;

  // Count-and-skip all the leading `0`s.
  while (true) {
    constexpr int batchSize = 32;
    pump.fill(batchSize);
    uint32_t batch = pump.peekBitsNoFill(batchSize);
    int numZerosInThisBatch = std::countl_zero(batch);
    count += numZerosInThisBatch;
    bool allZeroes = numZerosInThisBatch == batchSize;
    int numBitsToSkip = numZerosInThisBatch;
    if (!allZeroes)
      numBitsToSkip += 1; // Also skip the first `1`.
    pump.skipBitsNoFill(numBitsToSkip);
    if (!allZeroes)
      break; // We're done!
  }

  return count;
}

// Given two non-negative numbers, how many times must the second number
// be multiplied by 2, for it to become not smaller than the first number?
// We are operating on arithmetical numbers here, without overflows.
int RAWSPEED_READNONE fuji_compressed_block::bitDiff(int value1, int value2) {
  invariant(value1 >= 0);
  invariant(value2 > 0);

  int lz1 = std::countl_zero(static_cast<unsigned>(value1));
  int lz2 = std::countl_zero(static_cast<unsigned>(value2));
  int decBits = std::max(lz2 - lz1, 0);
  if ((value2 << decBits) < value1)
    ++decBits;
  return std::min(decBits, 15);
}

__attribute__((always_inline)) inline int
fuji_compressed_block::fuji_decode_sample(int grad, int interp_val,
                                          const FujiQTable& qtable,
                                          int_pair* gradSlot) {
  int gradient = std::abs(grad);

  int sampleBits = fuji_zerobits(*pump);

  int codeBits;
  int codeDelta;
  if (sampleBits < common_info.max_bits - qtable.raw_bits - 1) {
    codeBits = bitDiff(gradSlot[gradient].value1, gradSlot[gradient].value2);
    codeDelta = sampleBits << codeBits;
  } else {
    codeBits = qtable.raw_bits;
    codeDelta = 1;
  }

  int code = 0;
  pump->fill(32);
  if (codeBits)
    code = pump->getBitsNoFill(codeBits);
  code += codeDelta;

  if (code < 0 || code >= qtable.total_values) {
    ThrowRDE("fuji_decode_sample");
  }

  if (code & 1) {
    code = -1 - code / 2;
  } else {
    code /= 2;
  }

  gradSlot[gradient].value1 += std::abs(code);

  if (gradSlot[gradient].value2 == common_info.min_value) {
    gradSlot[gradient].value1 >>= 1;
    gradSlot[gradient].value2 >>= 1;
  }

  gradSlot[gradient].value2++;

  // Dequantization scale: 1 for lossless / q_base==0 (identical to the
  // original code's implicit "+= code"), (2*q_base+1) otherwise.
  const int scale = 2 * qtable.q_base + 1;
  if (grad < 0) {
    interp_val -= code * scale;
  } else {
    interp_val += code * scale;
  }

  // CONFIRMED against rawler's read_code() / fuji_decode_sample_even/odd:
  // this threshold/wrap logic matches exactly.
  const int lowThresh = -qtable.q_base;
  const int highThresh = qtable.q_base + qtable.q_point[4];
  const int wrap = qtable.total_values * scale;
  if (interp_val < lowThresh) {
    interp_val += wrap;
  } else if (interp_val > highThresh) {
    interp_val -= wrap;
  }

  if (interp_val < 0)
    return 0;

  return std::min(interp_val, qtable.q_point[4]);
}

__attribute__((always_inline)) inline int
fuji_compressed_block::fuji_quant_gradient(const FujiQTable& qtable, int v1,
                                           int v2) const {
  return (qtable.q_gradient_multi *
          qtable.qTableLookup(qtable.q_point[4] + v1)) +
         qtable.qTableLookup(qtable.q_point[4] + v2);
}

// Picks which table (main, or one of the 3 static lossy tables) applies
// at this pixel, based on local gradient magnitude. Always returns -1
// (main table) for lossless files -- identical to today's single-table
// behaviour.
//
// CONFIRMED against rawler's fuji_decompressor.rs (fuji_decode_sample_even/
// odd, lines ~953-961 / ~1015-1023): critically, static table index k is
// only even considered while k < mainQTable_q_base -- i.e. when the
// CURRENT main table's q_base is 0, no static table is ever eligible;
// when it's 3, all three are checked. This gate was missing from the
// initial draft and is what caused the bitstream desync seen in testing.
__attribute__((always_inline)) inline int
selectLossyTableIndex(const fuji_compressed_params& common_info,
                      const FujiDecompressor::FujiHeader& header,
                      int mainQTable_q_base, int diffSum) {
  if (header.isLossless())
    return -1;
  for (int k = 0; k != 3; ++k) {
    if (mainQTable_q_base < k + 1)
      break;
    if (diffSum <= common_info.staticLossyTables[k].max_grad)
      return k;
  }
  return -1;
}

__attribute__((always_inline)) inline std::tuple<int, int, int>
fuji_compressed_block::fuji_decode_interpolation_even_inner(xt_lines c,
                                                            int col) const {
  int Rb = lines(c - 1, 1 + (2 * (col + 0)) + 0);
  int Rc = lines(c - 1, 1 + (2 * (col - 1)) + 1);
  int Rd = lines(c - 1, 1 + (2 * (col + 0)) + 1);
  int Rf = lines(c - 2, 1 + (2 * (col + 0)) + 0);

  int diffRcRb = std::abs(Rc - Rb);
  int diffRfRb = std::abs(Rf - Rb);
  int diffRdRb = std::abs(Rd - Rb);

  int Term0 = 2 * Rb;
  int Term1;
  int Term2;
  if (diffRcRb > std::max(diffRfRb, diffRdRb)) {
    Term1 = Rf;
    Term2 = Rd;
  } else {
    if (diffRdRb > std::max(diffRcRb, diffRfRb)) {
      Term1 = Rf;
    } else {
      Term1 = Rd;
    }
    Term2 = Rc;
  }

  int interp_val = Term0 + Term1 + Term2;
  interp_val >>= 2;

  // CONFIRMED against rawler source: diffRfRb + diffRcRb is the correct
  // pairing for the even case.
  int tableIdx = selectLossyTableIndex(common_info, header, mainQTable.q_base,
                                       diffRfRb + diffRcRb);
  const FujiQTable& qtable =
      tableIdx == -1 ? mainQTable : common_info.staticLossyTables[tableIdx];

  int grad = fuji_quant_gradient(qtable, Rb - Rf, Rc - Rb);
  return {grad, interp_val, tableIdx};
}

__attribute__((always_inline)) inline std::tuple<int, int, int>
fuji_compressed_block::fuji_decode_interpolation_odd_inner(xt_lines c,
                                                           int col) const {
  int Ra = lines(c + 0, 1 + (2 * (col + 0)) + 0);
  int Rb = lines(c - 1, 1 + (2 * (col + 0)) + 1);
  int Rc = lines(c - 1, 1 + (2 * (col + 0)) + 0);
  int Rd = lines(c - 1, 1 + (2 * (col + 1)) + 0);
  int Rg = lines(c + 0, 1 + (2 * (col + 1)) + 0);

  int interp_val = (Ra + Rg);
  if (auto [min, max] = std::minmax(Rc, Rd); Rb < min || Rb > max) {
    interp_val += 2 * Rb;
    interp_val >>= 1;
  }
  interp_val >>= 1;

  // CONFIRMED against rawler source: |Rb-Rc| + |Rc-Ra| is the correct
  // pairing for the odd case.
  int diffRbRc = std::abs(Rb - Rc);
  int diffRcRa = std::abs(Rc - Ra);
  int tableIdx = selectLossyTableIndex(common_info, header, mainQTable.q_base,
                                       diffRbRc + diffRcRa);
  const FujiQTable& qtable =
      tableIdx == -1 ? mainQTable : common_info.staticLossyTables[tableIdx];

  int grad = fuji_quant_gradient(qtable, Rb - Rc, Rc - Ra);
  return {grad, interp_val, tableIdx};
}

__attribute__((always_inline)) inline int
fuji_compressed_block::fuji_decode_sample_even(xt_lines c, int col,
                                               int colorIdx) {
  auto [grad, interp_val, tableIdx] =
      fuji_decode_interpolation_even_inner(c, col);
  if (tableIdx == -1) {
    return fuji_decode_sample(grad, interp_val, mainQTable,
                              grad_even[colorIdx].data());
  }
  return fuji_decode_sample(grad, interp_val,
                            common_info.staticLossyTables[tableIdx],
                            grad_even_lossy[colorIdx][tableIdx].data());
}

__attribute__((always_inline)) inline int
fuji_compressed_block::fuji_decode_sample_odd(xt_lines c, int col,
                                              int colorIdx) {
  auto [grad, interp_val, tableIdx] =
      fuji_decode_interpolation_odd_inner(c, col);
  if (tableIdx == -1) {
    return fuji_decode_sample(grad, interp_val, mainQTable,
                              grad_odd[colorIdx].data());
  }
  return fuji_decode_sample(grad, interp_val,
                            common_info.staticLossyTables[tableIdx],
                            grad_odd_lossy[colorIdx][tableIdx].data());
}

__attribute__((always_inline)) inline int
fuji_compressed_block::fuji_decode_interpolation_even(xt_lines c,
                                                      int col) const {
  auto [grad, interp_val, tableIdx] =
      fuji_decode_interpolation_even_inner(c, col);
  return interp_val;
}

void fuji_compressed_block::fuji_extend_generic(int start, int end) const {
  for (int i = start; i <= end; i++) {
    lines(i, 0) = lines(i - 1, 1);
    lines(i, lines.width() - 1) = lines(i - 1, lines.width() - 2);
  }
}

void fuji_compressed_block::fuji_extend_red() const {
  fuji_extend_generic(R2, R4);
}

void fuji_compressed_block::fuji_extend_green() const {
  fuji_extend_generic(G2, G7);
}

void fuji_compressed_block::fuji_extend_blue() const {
  fuji_extend_generic(B2, B4);
}

template <typename T>
__attribute__((always_inline)) inline void
fuji_compressed_block::fuji_decode_block(T func_even,
                                         [[maybe_unused]] int cur_line) {
  invariant(common_info.line_width % 2 == 0);
  const int line_width = common_info.line_width / 2;

  auto pass = [this, &line_width, func_even](std::array<xt_lines, 2> c,
                                             int row) {
    int grad = row % 3;

    struct ColorPos final {
      int even = 0;
      int odd = 0;
    };

    std::array<ColorPos, 2> pos;
    for (int i = 0; i != line_width + 4; ++i) {
      if (i < line_width) {
        for (int comp = 0; comp != 2; comp++) {
          int& col = pos[comp].even;
          int sample = func_even(c[comp], col, grad, row, i, comp);
          lines(c[comp], 1 + (2 * col) + 0) = implicit_cast<uint16_t>(sample);
          ++col;
        }
      }

      if (i >= 4) {
        for (int comp = 0; comp != 2; comp++) {
          int& col = pos[comp].odd;
          int sample = fuji_decode_sample_odd(c[comp], col, grad);
          lines(c[comp], 1 + (2 * col) + 1) = implicit_cast<uint16_t>(sample);
          ++col;
        }
      }
    }
  };

  using Tag = BayerTag;
  const std::array<CFAColor, MCU<Tag>.x * MCU<Tag>.y> CFAData =
      getAsCFAColors(BayerPhase::RGGB);
  const Array2DRef<const CFAColor> CFA(CFAData.data(), MCU<Tag>.x, MCU<Tag>.y);

  std::array<int, 3> PerColorCounter;
  std::fill(PerColorCounter.begin(), PerColorCounter.end(), 0);
  auto ColorCounter = [&PerColorCounter](CFAColor c) -> int& {
    switch (c) {
      using enum CFAColor;
    case RED:
    case GREEN:
    case BLUE:
      return PerColorCounter[static_cast<uint8_t>(c)];
    default:
      __builtin_unreachable();
    }
  };

  auto CurLineForColor = [&ColorCounter](CFAColor c) {
    xt_lines res;
    switch (c) {
      using enum CFAColor;
    case RED:
      res = R2;
      break;
    case GREEN:
      res = G2;
      break;
    case BLUE:
      res = B2;
      break;
    default:
      __builtin_unreachable();
    }
    int& off = ColorCounter(c);
    res = static_cast<xt_lines>(res + off);
    ++off;
    return res;
  };

  for (int row = 0; row != 6; ++row) {
    CFAColor c0 = CFA(row % CFA.height(), /*col=*/0);
    CFAColor c1 = CFA(row % CFA.height(), /*col=*/1);
    pass({CurLineForColor(c0), CurLineForColor(c1)}, row);
    for (CFAColor c : {c0, c1}) {
      switch (c) {
      case CFAColor::RED:
        fuji_extend_red();
        break;
      case CFAColor::GREEN:
        fuji_extend_green();
        break;
      case CFAColor::BLUE:
        fuji_extend_blue();
        break;
      default:
        __builtin_unreachable();
      }
    }
  }
}

void fuji_compressed_block::xtrans_decode_block(int cur_line) {
  fuji_decode_block(
      [this](xt_lines c, int col, int colorIdx, int row, int i, int comp) {
        if ((comp == 0 && (row == 0 || (row == 2 && i % 2 == 0) ||
                           (row == 4 && i % 2 != 0) || row == 5)) ||
            (comp == 1 && (row == 1 || row == 2 || (row == 3 && i % 2 != 0) ||
                           (row == 5 && i % 2 == 0))))
          return fuji_decode_interpolation_even(c, col);
        invariant((comp == 0 && (row == 1 || (row == 2 && i % 2 != 0) ||
                                 row == 3 || (row == 4 && i % 2 == 0))) ||
                  (comp == 1 && (row == 0 || (row == 3 && i % 2 == 0) ||
                                 row == 4 || (row == 5 && i % 2 != 0))));
        return fuji_decode_sample_even(c, col, colorIdx);
      },
      cur_line);
}

void fuji_compressed_block::fuji_bayer_decode_block(int cur_line) {
  fuji_decode_block(
      [this](xt_lines c, int col, int colorIdx, [[maybe_unused]] int row,
             [[maybe_unused]] int i, [[maybe_unused]] int comp) {
        return fuji_decode_sample_even(c, col, colorIdx);
      },
      cur_line);
}

void fuji_compressed_block::fuji_decode_strip(const FujiStrip& strip,
                                              Array1DRef<const uint8_t> q_bases,
                                              int q_bases_offset) {
  const unsigned line_size = sizeof(uint16_t) * (common_info.line_width + 2);

  struct i_pair final {
    int a;
    int b;
  };

  const std::array<i_pair, 3> colors = {{{R0, 5}, {G0, 8}, {B0, 5}}};

  for (int cur_line = 0; cur_line < strip.height(); cur_line++) {
    if (!header.isLossless()) {
      int q_base = q_bases(q_bases_offset + cur_line);
      if (cur_line == 0 || q_base != mainQTable.q_base) {
        const int max_value = (1 << header.raw_bits) - 1;
        mainQTable = rebuildMainQTable(header, max_value, q_base);

        // Reset main-table gradient adaptation state to match the
        // rebuilt table -- same 41-bucket arrays used for lossless.
        for (int j = 0; j < 3; j++) {
          for (int i = 0; i < 41; i++) {
            grad_even[j][i].value1 = mainQTable.maxDiff;
            grad_even[j][i].value2 = 1;
            grad_odd[j][i].value1 = mainQTable.maxDiff;
            grad_odd[j][i].value2 = 1;
          }
        }
      }
    }

    if (header.raw_type == 16) {
      xtrans_decode_block(cur_line);
    } else {
      fuji_bayer_decode_block(cur_line);
    }

    if (header.raw_type == 16) {
      copy_line_to_xtrans(strip, cur_line);
    } else {
      copy_line_to_bayer(strip, cur_line);
    }

    if (cur_line + 1 == strip.height())
      break;

    // Last two lines of each color become the first two lines.
    for (auto i : colors) {
      memcpy(&lines(i.a, 0), &lines(i.a + i.b - 2, 0), 2 * line_size);
    }

    for (auto i : colors) {
      const auto out = CroppedArray2DRef(
          lines, /*offsetCols=*/0, /*offsetRows=*/i.a + 2,
          /*croppedWidth=*/lines.width(), /*croppedHeight=*/i.b - 2);

      // All other lines of each color become uninitialized.
      MSan::Allocated(out);

      // And the first (real, uninitialized) line of each color gets the content
      // of the last helper column from the last decoded sample of previous
      // line of that color.
      lines(i.a + 2, lines.width() - 1) = lines(i.a + 2 - 1, lines.width() - 2);
    }
  }
}

class FujiDecompressorImpl final {
  RawImage mRaw;
  const Array1DRef<const Array1DRef<const uint8_t>> strips;

  const FujiDecompressor::FujiHeader& header;

  const fuji_compressed_params common_info;

  // Lossy-only. Full array (all strips concatenated); q_bases_line_step
  // is the per-strip stride (each strip's run padded to a multiple of 16),
  // matching FujiDecompressor::FujiDecompressor()'s parsing layout.
  const Array1DRef<const uint8_t> q_bases;
  const int q_bases_line_step;

  void decompressThread() const noexcept;

public:
  FujiDecompressorImpl(RawImage mRaw,
                       Array1DRef<const Array1DRef<const uint8_t>> strips,
                       const FujiDecompressor::FujiHeader& h,
                       Array1DRef<const uint8_t> q_bases_,
                       int q_bases_line_step_);

  void decompress();
};

FujiDecompressorImpl::FujiDecompressorImpl(
    RawImage mRaw_, Array1DRef<const Array1DRef<const uint8_t>> strips_,
    const FujiDecompressor::FujiHeader& h_, Array1DRef<const uint8_t> q_bases_,
    int q_bases_line_step_)
    : mRaw(std::move(mRaw_)), strips(strips_), header(h_), common_info(header),
      q_bases(q_bases_), q_bases_line_step(q_bases_line_step_) {}

void FujiDecompressorImpl::decompressThread() const noexcept {
  fuji_compressed_block block_info(mRaw->getU16DataAsUncroppedArray2DRef(),
                                   header, common_info);

#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (int block = 0; block < header.blocks_in_row; ++block) {
    try {
      FujiStrip strip(header, block, strips(block));
      block_info.reset();
      block_info.pump = BitStreamerMSB(strip.input);
      block_info.fuji_decode_strip(strip, q_bases, block * q_bases_line_step);
    } catch (const RawspeedException& err) {
      // Propagate the exception out of OpenMP magic.
      mRaw->setError(err.what());
    } catch (...) {
      // We should not get any other exception type here.
      __builtin_unreachable();
    }
  }
}

void FujiDecompressorImpl::decompress() {
#ifdef HAVE_OPENMP
#pragma omp parallel default(none)                                             \
    num_threads(rawspeed_get_number_of_processor_cores())
#endif
  decompressThread();

  // WIP diagnostic -- cross-validation spot-check against an independent
  // decoder (e.g. dnglab/rawpy). Off by default; set the environment
  // variable RAWSPEED_FUJI_DIAG (to any non-empty value) before running
  // to enable it -- no rebuild needed to toggle. Uses relative image
  // fractions rather than fixed pixel coordinates so it works unmodified
  // on any file's dimensions. Remove entirely once cross-validation
  // testing is complete and this patch is ready to upstream.
  if (std::getenv("RAWSPEED_FUJI_DIAG") != nullptr) {
    const Array2DRef<uint16_t> img = mRaw->getU16DataAsUncroppedArray2DRef();
    fprintf(stderr, "DIAG pixel spot-check: img dims = %d x %d (w x h)\n",
            img.width(), img.height());
    const double fractions[] = {0.02, 0.25, 0.5, 0.75, 0.98};
    for (double rowFrac : fractions) {
      for (double colFrac : fractions) {
        int row = std::min(img.height() - 1,
                           static_cast<int>(rowFrac * img.height()));
        int col =
            std::min(img.width() - 1, static_cast<int>(colFrac * img.width()));
        fprintf(stderr, "DIAG (%d,%d) = %d\n", row, col, img(row, col));
      }
    }
  }

  std::string firstErr;
  if (mRaw->isTooManyErrors(1, &firstErr)) {
    ThrowRDE("Too many errors encountered. Giving up. First Error:\n%s",
             firstErr.c_str());
  }
}

} // namespace

FujiDecompressor::FujiDecompressor(RawImage img, ByteStream input_)
    : mRaw(std::move(img)), input(input_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  input.setByteOrder(Endianness::big);

  header = FujiHeader(input);
  if (!header)
    ThrowRDE("compressed RAF header check");

  if (mRaw->dim != iPoint2D(header.raw_width, header.raw_height))
    ThrowRDE("RAF header specifies different dimensions!");

  if (12 == header.raw_bits) {
    ThrowRDE("Aha, finally, a 12-bit compressed RAF! Please consider providing "
             "samples on <https://raw.pixls.us/>, thanks!");
  }

  if (mRaw->cfa.getSize() == iPoint2D(6, 6)) {
    Optional<XTransPhase> p = getAsXTransPhase(mRaw->cfa);
    if (!p)
      ThrowRDE("Invalid X-Trans CFA");
    if (p != iPoint2D(0, 0))
      ThrowRDE("Unexpected X-Trans phase: {%i,%i}. Please file a bug!", p->x,
               p->y);
  } else if (mRaw->cfa.getSize() == iPoint2D(2, 2)) {
    Optional<BayerPhase> p = getAsBayerPhase(mRaw->cfa);
    if (!p)
      ThrowRDE("Invalid Bayer CFA");
    if (p != BayerPhase::RGGB)
      ThrowRDE("Unexpected Bayer phase: %i. Please file a bug!",
               static_cast<int>(*p));
  } else {
    ThrowRDE("Unexpected CFA size");
  }

  // read block sizes
  std::vector<uint32_t> block_sizes;
  block_sizes.resize(header.blocks_in_row);
  for (auto& block_size : block_sizes)
    block_size = input.getU32();

  // some padding?
  if (const uint64_t raw_offset = sizeof(uint32_t) * header.blocks_in_row;
      raw_offset & 0xC) {
    const int padding = 0x10 - (raw_offset & 0xC);
    input.skipBytes(padding);
  }

  // For lossy-compressed RAFs, a per-line quantization base ("q_base")
  // follows the block offset table: one byte per line, per strip, with
  // each strip's run of q_bases padded up to a multiple of 16 bytes.
  // Mirrors rawler's decompress_fuji(): line_step = roundUp(total_lines, 16).
  if (!header.isLossless()) {
    const int lineStep = roundUpDivisionSafe(header.total_lines, 16) * 16;
    q_bases_line_step = lineStep;
    const int totalQBases = header.blocks_in_row * lineStep;
    q_bases.resize(totalQBases);
    for (auto& q_base : q_bases)
      q_base = input.getByte();
  }

  // calculating raw block offsets
  strips.reserve(header.blocks_in_row);

  for (const auto& block_size : block_sizes)
    strips.emplace_back(input.getStream(block_size).getAsArray1DRef());
}

void FujiDecompressor::decompress() const {
  // For lossless files, q_bases is legitimately empty (never parsed, never
  // read -- every access is gated by !header.isLossless()). But an empty
  // std::vector's .data() can return nullptr (does on libc++), and
  // Array1DRef's invariant check requires a non-null pointer
  // unconditionally, even at size 0. Passing q_bases.data() directly here
  // crashes on every lossless file. A single static sentinel byte keeps
  // the pointer non-null while size stays 0 -- it's never dereferenced
  // since nothing ever indexes into this array for a lossless file.
  static const uint8_t kEmptyQBasesSentinel = 0;

  FujiDecompressorImpl impl(
      mRaw,
      Array1DRef<const Array1DRef<const uint8_t>>(
          strips.data(), implicit_cast<Buffer::size_type>(strips.size())),
      header,
      Array1DRef<const uint8_t>(
          q_bases.empty() ? &kEmptyQBasesSentinel : q_bases.data(),
          implicit_cast<Buffer::size_type>(q_bases.size())),
      q_bases_line_step);
  impl.decompress();
}

FujiDecompressor::FujiHeader::FujiHeader(ByteStream& bs)
    : signature(bs.getU16()), version(bs.getByte()), raw_type(bs.getByte()),
      raw_bits(bs.getByte()), raw_height(bs.getU16()),
      raw_rounded_width(bs.getU16()), raw_width(bs.getU16()),
      block_size(bs.getU16()), blocks_in_row(bs.getByte()),
      total_lines(bs.getU16()),
      MCU(raw_type == 16 ? ::rawspeed::MCU<XTransTag>
                         : ::rawspeed::MCU<BayerTag>) {}

FujiDecompressor::FujiHeader::operator bool() const {
  // general validation
  const bool invalid =
      (signature != 0x4953 ||
       (version != 0 && version != 1) || // 0 = lossy, 1 = lossless.
                                         // CONFIRMED: real lossy RAF
                                         // samples parse and decode
                                         // pixel-exact with version==0.
       raw_height > 0x3000 || raw_height < FujiStrip::lineHeight() ||
       raw_height % FujiStrip::lineHeight() || raw_width > 0x3000 ||
       raw_width < 0x300 || raw_width % 24 || raw_rounded_width > 0x3000 ||
       block_size != 0x300 || raw_rounded_width < block_size ||
       raw_rounded_width % block_size ||
       raw_rounded_width - raw_width >= block_size || blocks_in_row > 0x10 ||
       blocks_in_row == 0 || blocks_in_row != raw_rounded_width / block_size ||
       blocks_in_row != roundUpDivisionSafe(raw_width, block_size) ||
       total_lines > 0x800 || total_lines == 0 ||
       total_lines != raw_height / FujiStrip::lineHeight() ||
       (raw_bits != 12 && raw_bits != 14 && raw_bits != 16) ||
       (raw_type != 16 && raw_type != 0));

  return !invalid;
}

} // namespace rawspeed
