/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2026 Kabir Kwatra

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

// This is a clean-room implementation of the Sony "ARW 6.0" codec (the camera's
// "Compressed RAW 2" lossy mode, TIFF compression 32766), used by the A7R VI
// (ILCE-7RM6) and A7 V (ILCE-7M5). The bitstream format was recovered by
// observing the decoder's input/output behaviour, and the implementation is
// validated byte-exact (MAE 0) against that reference output.
//
// Pipeline:
//  container strip -> 1-4 spatial tiles (2x2 grid for a large frame; a single
//                     tile when the frame fits in one)
//   per tile -> picture header (per-resolution byte offsets)
//            -> 4 components: Y-detail, Y-LL, Cb, Cr
//   per component -> 5 resolutions (1 LL0 + 3 detail levels + [Y-LL])
//            -> per-row framing table (offset/length/qbits)
//            -> GCLI bitplane entropy decode
//            -> inverse reversible 5/3 wavelet (LL0 + 3 detail levels)
//   luma -> two side-by-side base wavelet planes combined by a post-filter
//        -> log->linear map: out = LUT[clamp(coeff + 2048)]
//   chroma -> colour-convert against the luma reference
//   YCC -> RGGB mosaic compose + 2x2 tile placement

#include "rawspeedconfig.h"
#include "decompressors/SonyArw6Decompressor.h"
#include "adt/Array2DRef.h"
#include "adt/Casts.h"
#include "adt/DefaultInitAllocatorAdaptor.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/SonyArw6LogToLinear.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace rawspeed {

namespace {

// ------------------------------------------------------------------------- //
// Code: the ARW6 bit reader. Big-endian 64-bit words, MSB-first,
// with a total bit budget. Single-bit reads consume the accumulator MSB-first;
// a fresh word's 64 bits are consumed across 64 reads (bit indices 63..0).
// ------------------------------------------------------------------------- //
class Code final {
  const uint8_t* buf;
  int len;

  uint64_t acc = 0;
  int wi = 0;     // index of next byte-word to load
  int nwords;     // remaining whole 64-bit words still loadable
  int bitpos = 0; // index of the bit being consumed

public:
  int flag = 0; // 0 = ok, 2 = exhausted
  int budget;   // total remaining bit budget (countdown)

  Code(const uint8_t* buf_, int len_)
      : buf(buf_), len(len_), nwords((len_ + 7) >> 3), budget(len_ * 8) {}

  // Load the next 64-bit big-endian word; returns false if out of words.
  bool refill() {
    if (nwords <= 0) {
      flag = 2;
      return false;
    }
    nwords -= 1;
    uint64_t w = 0;
    for (int b = 0; b < 8; ++b) {
      int idx = (wi * 8) + b;
      uint64_t byte = (idx < len) ? static_cast<uint64_t>(buf[idx]) : 0;
      w = (w << 8) | byte;
    }
    acc = w;
    wi += 1;
    bitpos = 63;
    return true;
  }

  // The canonical single-bit MSB-first read.
  int readBit() {
    if (budget <= 0) {
      flag = 2;
      return 0;
    }
    budget -= 1;
    int old = bitpos;
    bitpos = old - 1;
    if (old > 0)
      return static_cast<int>((acc >> (old - 1)) & 1U);
    if (!refill())
      return 0;
    return static_cast<int>((acc >> 63) & 1U);
  }

  // Read n (0..56) bits MSB-first; bit-for-bit equivalent to n readBit() calls.
  // Fast path when all n bits lie within the current word and budget allows;
  // otherwise falls back to per-bit reads (identical boundary/flag behaviour).
  uint64_t readBitsMsb(int n) {
    if (n <= bitpos && budget >= n) {
      budget -= n;
      const int newpos = bitpos - n;
      const uint64_t v = (acc >> newpos) & ((UINT64_C(1) << n) - 1U);
      bitpos = newpos;
      return v;
    }
    uint64_t v = 0;
    for (int i = 0; i < n; ++i)
      v = (v << 1) | static_cast<uint64_t>(readBit());
    return v;
  }
};

// ------------------------------------------------------------------------- //
// Update the running significance/qbits `run`.
//   read bit1; 0 -> return unchanged.
//   bit1==1, read bit2:
//     bit2==1 -> DECREMENT: run-=1 (clamp 0); while next bit==0: run-=1.
//     bit2==0 -> INCREMENT: run += 1 + (#0-bits until a 1).
// ------------------------------------------------------------------------- //
void updateBits(Code& code, int& run) {
  int budget0 = code.budget;
  if (budget0 <= 0) {
    code.flag = 2;
    run = 0;
    return;
  }

  int bit1 = code.readBit();
  if (bit1 == 0)
    return; // significant at current run
  if (code.flag != 0)
    return;

  if (budget0 == 1) {
    code.flag = 2; // consumed the only budget on bit1; can't read bit2
    return;
  }
  int bit2 = code.readBit();
  if (code.flag != 0 && bit2 == 0) {
    // refill failure on bit2 zeroes run (matches the reference on exhaustion).
    run = 0;
    return;
  }

  if (bit2 != 0) {
    // ---- DECREMENT path ----
    int r = run - 1;
    if (r < 0) {
      run = 0;
      return;
    }
    run = r;
    if (r == 0)
      return;
    while (true) {
      if (code.budget <= 0) {
        code.flag = 2;
        run = 0;
        return;
      }
      int bit = code.readBit();
      if (bit)
        return; // 1-bit terminates
      if (code.flag != 0) {
        run = 0;
        return;
      }
      run -= 1;
      if (run <= 0)
        return;
    }
  } else {
    // ---- INCREMENT path ----  run += 1 + zeros until a 1-bit
    if (code.budget <= 0) {
      code.flag = 2;
      return; // run unchanged
    }
    int neg = -1;
    while (true) {
      if (code.budget <= 0) {
        code.flag = 2;
        return; // run unchanged on exhaustion
      }
      int bit = code.readBit();
      if (code.flag != 0 && bit == 0)
        return; // refill failure -> run unchanged
      if (bit) {
        run += (-neg);
        return;
      }
      neg -= 1;
    }
  }
}

// Inlined re-read used by code2coef2Row after a zero-group run (run was 0).
// Reads a unary code: run = 1 + (#leading 0-bits before a 1-bit).
void inlineReadRun0(Code& code, int& run) {
  if (code.budget <= 0) {
    code.flag = 2;
    return;
  }
  int neg = -1;
  while (true) {
    if (code.budget <= 0) {
      code.flag = 2;
      return;
    }
    int bit = code.readBit();
    if (code.flag != 0 && bit == 0)
      return;
    if (bit) {
      run = -neg;
      return;
    }
    neg -= 1;
  }
}

// Read 4 magnitudes packed as one 4*qbits-wide MSB-first field (top group =
// out[0]). If signFlag>0, applies GCLI refinement per magnitude.
void decodeMagRev(Code& code, std::array<int32_t, 4>& out, int qbits,
                  int signFlag) {
  // `qbits` is the running significance count, which `updateBits` can drive
  // arbitrarily high on malformed input; a real magnitude field is tiny. Reject
  // anything that could make the shifts below undefined (a sane bound well
  // above any valid value and below the 64-bit shift-width limit).
  if (qbits < 0 || qbits >= 22)
    ThrowRDE("ARW6: invalid magnitude bit-width %d", qbits);
  std::array<int32_t, 4> vals = {0, 0, 0, 0};
  if (qbits > 0) {
    int total = 4 * qbits;
    uint64_t word = code.readBitsMsb(total);
    uint64_t mask = (1ULL << qbits) - 1;
    for (int k = 0; k < 4; ++k)
      vals[k] = static_cast<int32_t>((word >> ((3 - k) * qbits)) & mask);
  }
  if (signFlag > 0) {
    int sh = signFlag - 1;
    for (int k = 0; k < 4; ++k) {
      int64_t v = vals[k];
      if (v >= 1) {
        int64_t r = (((v << 1) | 1) << sh) - (v & 1);
        vals[k] = (r < 0x7fffffff) ? static_cast<int32_t>(r) : 0x7fffffff;
      }
    }
  }
  out = vals;
}

// One sign bit per magnitude >= 1.
void decodeSignRev(Code& code, std::array<int32_t, 4>& out) {
  for (int k = 0; k < 4; ++k) {
    int32_t v = out[k];
    if (v >= 1) {
      int s = code.readBit();
      // 64-bit intermediate: v can reach the INT32 saturation value from the
      // magnitude refinement, where `2 * v` would overflow int.
      out[k] = static_cast<int32_t>(v - (int64_t{2} * v * s));
    }
  }
}

// Decode one subband row into out[ngroups*4] (4 int32 coeffs/group).
void code2coef2Row(Code& code, int m, int ngroups, std::vector<int32_t>& out) {
  int n = ngroups * 4;
  out.assign(n, 0);
  if (code.flag != 0)
    return;

  int run = 0;
  updateBits(code, run);

  if (ngroups - 1 < 0)
    return;

  int g = 0;  // current group index
  int oi = 0; // output index = g*4
  while (g < ngroups) {
    if (run != 0) {
      // ---- significant group ----
      std::array<int32_t, 4> grp = {0, 0, 0, 0};
      decodeMagRev(code, grp, run, m);
      if (g < ngroups - 1)
        updateBits(code, run);
      decodeSignRev(code, grp);
      out[oi] = grp[0];
      out[oi + 1] = grp[1];
      out[oi + 2] = grp[2];
      out[oi + 3] = grp[3];
      if (code.flag != 0)
        return; // remaining groups already zero
      g += 1;
      oi += 4;
    } else {
      // ---- zero-group run (exp-golomb skip) ----
      int remaining = ngroups - g;
      if (remaining < 2)
        break;
      int k = 0;
      int value = 1; // = 2^k tracker
      bool terminated = false;
      while (true) {
        if (code.budget <= 0) {
          code.flag = 2;
          break;
        }
        int bit = code.readBit();
        if (code.flag != 0 && bit == 0) {
          code.flag = 2;
          break;
        }
        if (bit) {
          terminated = true;
          break;
        }
        // bit == 0: extend exponent
        value = 2 << k;
        k += 1;
        if (value >= remaining)
          break;
      }
      if (!terminated)
        break;
      // read k mantissa bits
      int mant = 0;
      for (int i = 0; i < k; ++i) {
        if (code.budget <= 0) {
          code.flag = 2;
          mant = (mant << 1);
          continue;
        }
        mant = (mant << 1) | code.readBit();
      }
      int skip = value + ((k > 0) ? (mant & ((1 << k) - 1)) : 0);
      g += skip;
      oi += skip * 4;
      if (g >= ngroups)
        break;
      // inlined re-read of next run (run was 0)
      run = 0;
      inlineReadRun0(code, run);
      if (code.flag != 0)
        break;
      if (run >= 0x14)
        break;
    }
  }
}

// CoefDiffDecode: horizontal cumulative (delta) decode with int16 wraparound.
void coefDiffDecode(std::vector<int32_t>& coef, int n) {
  if (n < 2)
    return;
  int32_t acc = coef[0];
  for (int i = 1; i < n; ++i) {
    int32_t s =
        static_cast<int32_t>(static_cast<uint32_t>(acc + coef[i]) & 0xffffU);
    if (s >= 0x8000)
      s -= 0x10000;
    coef[i] = s;
    acc = s;
  }
}

// ------------------------------------------------------------------------- //
// A 2D row-major grid. `Grid` (int16) holds the wavelet/entropy coefficients,
// which are always wrapped into signed-16-bit range; `Plane` (uint16) holds the
// post-LUT output planes (Y/Cb/Cr), whose values can reach the LUT ceiling
// (~39k, above the 32800 white point) and so need the wider unsigned type.
// ------------------------------------------------------------------------- //
template <typename T> struct GridT {
  // Skip-zeroing allocator (as VC5Decompressor uses): the no-argument vector
  // ctor leaves storage uninitialized. `GridT(w, h)` still zero-fills via the
  // (n, 0) ctor for the callers that rely on unwritten cells reading 0 (the
  // entropy subband grids, Y-LL resize, luma seam); `GridT(w, h, NoInit{})`
  // skips the fill for grids whose every cell is written before any read.
  std::vector<T, DefaultInitAllocatorAdaptor<T>> d;
  int w = 0;
  int h = 0;

  struct NoInit {};

  GridT() = default;
  GridT(int w_, int h_) : d(static_cast<size_t>(w_) * h_, 0), w(w_), h(h_) {}
  GridT(int w_, int h_, NoInit)
      : d(static_cast<size_t>(w_) * h_), w(w_), h(h_) {}

  T& at(int y, int x) { return d[static_cast<size_t>(y) * w + x]; }
  [[nodiscard]] T at(int y, int x) const {
    return d[static_cast<size_t>(y) * w + x];
  }
};

using Grid = GridT<int16_t>;   // wavelet / entropy coefficients
using Plane = GridT<uint16_t>; // post-LUT output planes

// Reduce a value modulo 2^16 into signed-16-bit range (C++20 two's complement).
inline int16_t wrapI16(int64_t v) { return static_cast<int16_t>(v); }

// A bounded view of a component's bitstream bytes.
struct CompBytes {
  const uint8_t* data;
  int len;
};

// ------------------------------------------------------------------------- //
// Framing parsers.
// ------------------------------------------------------------------------- //

// A picture-header record: per-TU byte offsets into the component bitstream.
struct PicRecord {
  int count = 0;
  std::array<int, 9> offsets{};
};

// Parse the picture header. `region` = the full tile region (the 16-byte
// sub-header is part of it); the picture-info buffer = region[0x10:].
// Returns up to 5 records, each with per-TU byte offsets into the component
// bitstream (which begins at region[0x80]).
std::array<PicRecord, 5> parsePictureHeader(const uint8_t* region,
                                            int regionLen) {
  auto u24 = [&](int o) -> int {
    int a = 0x10 + o;
    if (a + 2 >= regionLen)
      ThrowRDE("ARW6 picture header truncated");
    return (region[a] << 16) | (region[a + 1] << 8) | region[a + 2];
  };

  std::array<int, 4> bases{};
  for (int kk = 0; kk < 4; ++kk)
    bases[kk] = u24(kk * 3) << 4;

  std::array<PicRecord, 5> recs{};
  std::array<int, 5> counts{};
  std::array<std::array<int, 9>, 5> lens{};
  int nrec = 0;
  int off = 0x20;
  for (int res = 0; res < 5; ++res) {
    int a = 0x10 + off;
    if (a >= regionLen)
      break;
    int cnt = region[a] & 0xf;
    if (cnt > 9)
      break;
    int p = off + 1;
    for (int i = 0; i < cnt; ++i) {
      lens[res][i] = u24(p) << 4;
      p += 3;
    }
    counts[res] = cnt;
    nrec += 1;
    off += (cnt <= 4) ? 0x10 : 0x20;
  }

  // cumulative per-resolution base offsets (running add). Accumulate in 64
  // bits: the file-derived bases/lengths can each be ~2^28, so summing them in
  // int would overflow. Offsets that land outside the region are clamped to a
  // sentinel (regionLen) so the bounds checks in the consumers reject them.
  std::array<int64_t, 5> rb{};
  rb[0] = 0;
  rb[1] = bases[0];
  rb[2] = int64_t{bases[1]} + rb[1];
  rb[3] = int64_t{bases[2]} + rb[2];
  rb[4] = int64_t{bases[3]} + rb[3];

  for (int i = 0; i < nrec; ++i) {
    recs[i].count = counts[i];
    int64_t run = rb[i];
    for (int x = 0; x < counts[i]; ++x) {
      recs[i].offsets[x] =
          (run >= 0 && run < regionLen) ? static_cast<int>(run) : regionLen;
      run += lens[i][x];
    }
  }
  return recs;
}

// A per-row framing entry.
struct FrameRow {
  int byteOff;              // byte offset relative to the TU offset
  int length;               // entropy data byte length
  std::array<int, 3> qbits; // per-subband 4-bit qbits (w23 of them)
};

struct ResTable {
  int headerLen = 0;
  int nRows = 0;
  int w23 = 0;
  std::vector<FrameRow> rows;
};

// Parse the per-row framing table.
// `buf` = component bitstream; `off` = this TU's byte offset.
ResTable parseResolutionTable(const uint8_t* buf, int bufLen, int off) {
  // Subtraction form (and the off<0 guard) so a negative/overflowed offset
  // cannot pass via signed overflow of off+16.
  if (off < 0 || off > bufLen - 16)
    ThrowRDE("ARW6 resolution header out of bounds");
  const uint8_t* h = buf + off;
  int sz16 = (h[0] << 8) | h[1];
  int hlen = (sz16 << 4) + 0x10;
  if (h[5] != 0x40 || h[9] != 0x10)
    ThrowRDE("ARW6 resolution header marker mismatch");
  int w23 = (h[6] >> 6) & 3;
  int nRows = (h[7] << 8) | h[8];
  if (nRows > 0x200)
    ThrowRDE("ARW6 too many rows: %i", nRows);

  // The per-row table is a MSB-first BE bitstream starting at byte 16. Bound
  // the reads by the available bytes in the TU header.
  int tabBytesAvail = bufLen - (off + 16);
  int bitp = 0;
  auto rd = [&](int nbits) -> int {
    int v = 0;
    for (int i = 0; i < nbits; ++i) {
      int byteIdx = 16 + (bitp >> 3);
      if (byteIdx >= 16 + tabBytesAvail || off + byteIdx >= bufLen)
        ThrowRDE("ARW6 framing table out of bounds");
      int bit = (h[byteIdx] >> (7 - (bitp & 7))) & 1;
      v = (v << 1) | bit;
      bitp += 1;
    }
    return v;
  };

  ResTable t;
  t.headerLen = hlen;
  t.nRows = nRows;
  t.w23 = w23;
  t.rows.reserve(nRows);
  int cur = hlen;
  for (int i = 0; i < nRows; ++i) {
    FrameRow r{};
    r.length = rd(16);
    for (int j = 0; j < w23; ++j)
      r.qbits[j] = rd(4);
    r.byteOff = cur;
    t.rows.push_back(r);
    cur += r.length;
  }
  return t;
}

// ------------------------------------------------------------------------- //
// Per-resolution subband geometry: {slot -> (W, y0, y1, yoff)}.
// ------------------------------------------------------------------------- //
struct SubGeom {
  int W;
  int y0;
  int y1;
  int yoff;
};

// Subband slot indices: S20=LL, S28=HL (highH,lowV), S30=LH (lowH,highV),
// S38=HH (highH,highV). (Names are the byte offsets they occupy in the stream.)
enum Slot : uint8_t { S20 = 0, S28 = 1, S30 = 2, S38 = 3 };

// chroma_geometry(Wb): the FF/generic dyadic pyramid geometry. Derived from the
// validated reference (Wb = LL0 width). The y-ranges are a function of the
// component height BR (block-rows): LL0 height = ceil(BR/8)+1 rows in [2, ...].
// For the validated FF case (BR=1668, Wb=313) this reproduces the reference
// exactly. For other heights the same dyadic relations are used.
struct LevelGeom {
  // active slots; S20 for level 0, S28/S30/S38 for detail levels.
  std::array<SubGeom, 4> g{};
  std::array<bool, 4> active{};
};

// Canvas origin for the line-based 5/3 wavelet: the smallest top margin a>=6
// such that the symmetric-extended height (BR + 2a) is congruent to 8 (mod 16).
// Gives a=10 for FF (BR=1668) and a=7 for APS-C (BR=2186) -- and is what all
// the per-subband y-coordinates are derived from.
int arw6CanvasOrigin(int BR) {
  // 2*a is even, so a solution only exists for even BR; the residue repeats
  // with period 16, so it is found within 8 steps. Bounded to avoid spinning
  // forever on a malformed (odd) BR (callers also reject odd BR up front).
  for (int a = 6; a < 6 + 16; ++a) {
    if ((((2 * a) + BR) % 16) == 8)
      return a;
  }
  ThrowRDE("ARW6: no canvas origin for BR=%d", BR);
}

// Build the per-resolution subband geometry for a component of LL0-width Wb and
// height BR: a dyadic 3-level inverse-5/3 pyramid over the canvas extent
// [a, a+BR], with vLow = ceil-split and vHigh = floor-split at each level.
// Reproduces the reference geometry byte-exact for both FF (BR=1668, Wb=313)
// and APS-C (BR=2186, Wb=412).
std::array<LevelGeom, 4> chromaGeometry(int Wb, int BR) {
  const int a = arw6CanvasOrigin(BR);
  // Three dyadic splits of the extent [u0,u1); each split's low half feeds the
  // next. split[0] -> res3, split[1] -> res2, split[2] -> res1 (and res0 = LL,
  // which shares the deepest split). Each entry is
  // {vLowY0,vLowY1,vHighY0,vHighY1}.
  std::array<std::array<int, 4>, 3> sp{};
  int u0 = a;
  int u1 = a + BR;
  for (auto& s : sp) {
    const int l0 = (u0 + 1) / 2;
    const int l1 = (u1 + 1) / 2; // ceil
    const int h0 = u0 / 2;
    const int h1 = u1 / 2; // floor
    s = {l0, l1, h0, h1};
    u0 = l0;
    u1 = l1;
  }
  const std::array<int, 4> resSplit = {2, 2, 1, 0}; // res r -> split index
  const std::array<int, 4> wr = {Wb, Wb, 2 * Wb, 4 * Wb};
  const std::array<int, 4> yoffLow = {1, 1, 1, 2};
  const std::array<int, 4> yoffHigh = {1, 0, 1, 2};
  std::array<LevelGeom, 4> out{};
  for (int r = 0; r < 4; ++r) {
    const auto& s = sp[resSplit[r]];
    out[r].g[S20] = {wr[r], s[0], s[1], yoffLow[r]};  // LL  (vLow)
    out[r].g[S28] = {wr[r], s[0], s[1], yoffLow[r]};  // HL  (vLow)
    out[r].g[S30] = {wr[r], s[2], s[3], yoffHigh[r]}; // LH  (vHigh)
    out[r].g[S38] = {wr[r], s[2], s[3], yoffHigh[r]}; // HH  (vHigh)
    if (r == 0)
      out[r].active[S20] = true;
    else
      out[r].active[S28] = out[r].active[S30] = out[r].active[S38] = true;
  }
  return out;
}

// Per-rowgroup band-decode parameters.
struct BandTask {
  int height; // subband rows produced per rowgroup
  int rg;     // current rowgroup index
  int flag;   // bit0 set -> apply CoefDiffDecode (LL0 path)
  int m;      // per-band qbits for the magnitude decode
};

// Decode the rows of one subband band produced by rowgroup `t.rg` from the
// (shared) rowgroup Code into `grid`. CoefDiffDecode is applied iff t.flag bit0
// is set (the LL0 path). Stores wrap to signed 16-bit.
void decodeBandRows(Code& code, const SubGeom& sg, Grid& grid,
                    const BandTask& t, std::vector<int32_t>& scratch) {
  int w8 = sg.yoff + (t.height * t.rg);
  int r0 = std::max(w8, sg.y0);
  int r1 = std::min(w8 + t.height, sg.y1);
  int G = (sg.W + 3) >> 2;
  for (int r = r0; r < r1; ++r) {
    code2coef2Row(code, t.m, G, scratch);
    if (t.flag & 1)
      coefDiffDecode(scratch, sg.W);
    int gy = r - sg.y0;
    for (int x = 0; x < sg.W; ++x)
      grid.at(gy, x) = wrapI16(scratch[x]);
  }
}

// Decode one resolution (one TU) into its subband grids.
// level==0 & is_ll==0 : LL path, single band S20 (flag=1), height=1
// level==0 & is_ll==1 : LL path, single band S20 (flag=0), height=8
// level>0             : detail, 3 bands S28,S30,S38 (flag=0),
// height=1<<(level-1)
void decodeResolution(CompBytes comp, int off, int level, int isLl,
                      const LevelGeom& geom, std::array<Grid, 4>& grids) {
  ResTable tab = parseResolutionTable(comp.data, comp.len, off);

  // allocate active grids
  for (int s = 0; s < 4; ++s) {
    if (geom.active[s])
      grids[s] = Grid(geom.g[s].W, geom.g[s].y1 - geom.g[s].y0);
  }

  int height;
  int flag;
  std::array<int, 3> order{};
  int nOrder;
  if (level == 0 && isLl == 0) {
    height = 1;
    flag = 1;
    order = {S20, 0, 0};
    nOrder = 1;
  } else if (level == 0 && isLl == 1) {
    height = 8;
    flag = 0;
    order = {S20, 0, 0};
    nOrder = 1;
  } else {
    height = 1 << (level - 1);
    flag = 0;
    order = {S28, S30, S38};
    nOrder = 3;
  }

  std::vector<int32_t> scratch;
  for (int rg = 0; rg < tab.nRows; ++rg) {
    const FrameRow& row = tab.rows[rg];
    if (row.length == 0)
      continue;
    int dataOff = off + row.byteOff;
    if (dataOff < 0 || row.length < 0 || dataOff > comp.len - row.length)
      ThrowRDE("ARW6 entropy buffer out of bounds");
    // One fresh Code per rowgroup, SHARED across the (up to 3) bands, which are
    // decoded sequentially (HL, LH, HH) from the same bitstream.
    Code code(comp.data + dataOff, row.length);
    for (int si = 0; si < nOrder; ++si) {
      int slot = order[si];
      if (!geom.active[slot])
        continue;
      // The per-band qbits for the magnitude decode = the per-row 4-bit field.
      int m = 0;
      if (si < tab.w23)
        m = row.qbits[si];
      else if (tab.w23 > 0)
        m = row.qbits[0];
      decodeBandRows(code, geom.g[slot], grids[slot], {height, rg, flag, m},
                     scratch);
    }
  }
}

// ------------------------------------------------------------------------- //
// Inverse reversible 5/3 transform (interleaved, mirror boundary, int16 wrap).
// ------------------------------------------------------------------------- //

// 1D inverse 5/3 along a contiguous interleaved buffer S of length N.
// low_even selects whether the LOW band lands on even positions.
void inv53_1d(std::vector<int32_t>& S, int N, bool lowEven) {
  if (N <= 1) {
    for (int i = 0; i < N; ++i)
      S[i] = wrapI16(S[i]);
    return;
  }
  // int32 suffices: values are int16-range, so the lift sums (two int16 + 2)
  // never exceed int32.
  auto sm = [&](int i) -> int { return (i - 1 >= 0) ? S[i - 1] : S[1]; };
  auto sp = [&](int i) -> int { return (i + 1 < N) ? S[i + 1] : S[N - 2]; };
  int loStart = lowEven ? 0 : 1;
  int hiStart = lowEven ? 1 : 0;
  // predict (LOW): L -= (S[i-1]+S[i+1]+2)>>2
  for (int i = loStart; i < N; i += 2)
    S[i] = wrapI16(S[i] - ((sm(i) + sp(i) + 2) >> 2));
  // update (HIGH): H += (S[i-1]+S[i+1])>>1
  for (int i = hiStart; i < N; i += 2)
    S[i] = wrapI16(S[i] + ((sm(i) + sp(i)) >> 1));
}

// Inverse 5/3 combining two bands into a single grid along the given axis.
// axis 0 = vertical (interleave rows), axis 1 = horizontal (interleave cols).
Grid inv53Axis(const Grid& low, const Grid& high, int axis, bool lowEven) {
  bool horiz = (axis == 1);
  int nL = horiz ? low.w : low.h;
  int nH = horiz ? high.w : high.h;
  int span = horiz ? low.h : low.w; // number of independent 1D lines
  int N = nL + nH;
  int loBase = lowEven ? 0 : 1;
  int hiBase = lowEven ? 1 : 0;
  // Every cell is written by the scatter below, so skip zero-init.
  Grid out =
      horiz ? Grid(N, span, Grid::NoInit{}) : Grid(span, N, Grid::NoInit{});
  std::vector<int32_t> S(N);
  for (int line = 0; line < span; ++line) {
    if (horiz) {
      for (int i = 0; i < nL; ++i)
        S[loBase + 2 * i] = low.at(line, i);
      for (int i = 0; i < nH; ++i)
        S[hiBase + 2 * i] = high.at(line, i);
    } else {
      for (int i = 0; i < nL; ++i)
        S[loBase + 2 * i] = low.at(i, line);
      for (int i = 0; i < nH; ++i)
        S[hiBase + 2 * i] = high.at(i, line);
    }
    inv53_1d(S, N, lowEven);
    for (int k = 0; k < N; ++k) {
      if (horiz)
        out.at(line, k) = static_cast<int16_t>(S[k]);
      else
        out.at(k, line) = static_cast<int16_t>(S[k]);
    }
  }
  return out;
}

// Vertical inverse 5/3 specialised to be cache-friendly: interleave the
// low/high rows into the output, then lift across whole rows (sequential
// sweeps) instead of the per-column strided gather/scatter inv53Axis(axis=0)
// would do. Bit-for- bit identical to inv53Axis(low, high, 0, lowEven): low.w
// == high.w holds for every vertical combine (sibling subbands share a width),
// and the lifting reads LOW from HIGH rows (predict) then HIGH from the
// just-predicted LOW rows (update), exactly as inv53_1d does on the interleaved
// buffer.
Grid inv53Vertical(const Grid& low, const Grid& high, bool lowEven) {
  const int span = low.w;
  const int nL = low.h;
  const int nH = high.h;
  const int N = nL + nH;
  const int loBase = lowEven ? 0 : 1;
  const int hiBase = lowEven ? 1 : 0;
  // For N >= 2 every row is written (the interleave covers all rows, then the
  // lifting overwrites in place), so skip zero-init. The degenerate N <= 1 path
  // reads cells back during its wrap, so keep it zero-filled there.
  Grid out = (N >= 2) ? Grid(span, N, Grid::NoInit{}) : Grid(span, N);

  for (int i = 0; i < nL; ++i) {
    const int16_t* src = &low.d[static_cast<size_t>(i) * low.w];
    int16_t* dst = &out.d[static_cast<size_t>(loBase + 2 * i) * span];
    for (int x = 0; x < span; ++x)
      dst[x] = src[x];
  }
  for (int i = 0; i < nH; ++i) {
    const int16_t* src = &high.d[static_cast<size_t>(i) * high.w];
    int16_t* dst = &out.d[static_cast<size_t>(hiBase + 2 * i) * span];
    for (int x = 0; x < span; ++x)
      dst[x] = src[x];
  }

  if (N <= 1) {
    for (int x = 0; x < N * span; ++x)
      out.d[x] = wrapI16(out.d[x]);
    return out;
  }

  const int loStart = lowEven ? 0 : 1;
  const int hiStart = lowEven ? 1 : 0;
  // predict (LOW rows): row -= (above + below + 2) >> 2
  for (int i = loStart; i < N; i += 2) {
    const int im = (i - 1 >= 0) ? (i - 1) : 1;
    const int ip = (i + 1 < N) ? (i + 1) : (N - 2);
    int16_t* row = &out.d[static_cast<size_t>(i) * span];
    const int16_t* a = &out.d[static_cast<size_t>(im) * span];
    const int16_t* b = &out.d[static_cast<size_t>(ip) * span];
    for (int x = 0; x < span; ++x)
      row[x] = wrapI16(row[x] - ((a[x] + b[x] + 2) >> 2));
  }
  // update (HIGH rows): row += (above + below) >> 1
  for (int i = hiStart; i < N; i += 2) {
    const int im = (i - 1 >= 0) ? (i - 1) : 1;
    const int ip = (i + 1 < N) ? (i + 1) : (N - 2);
    int16_t* row = &out.d[static_cast<size_t>(i) * span];
    const int16_t* a = &out.d[static_cast<size_t>(im) * span];
    const int16_t* b = &out.d[static_cast<size_t>(ip) * span];
    for (int x = 0; x < span; ++x)
      row[x] = wrapI16(row[x] + ((a[x] + b[x]) >> 1));
  }
  return out;
}

// One 2D inverse 5/3 level: VERTICAL first, then HORIZONTAL.
Grid inv53Level(const Grid& LL, const Grid& HL, const Grid& LH, const Grid& HH,
                bool vlowEven) {
  Grid Lcol = inv53Vertical(LL, LH, vlowEven); // low-horizontal cols
  Grid Hcol = inv53Vertical(HL, HH, vlowEven); // high-horizontal cols
  return inv53Axis(Lcol, Hcol, /*axis=*/1, /*lowEven=*/true);
}

// Reconstruct the full dyadic pyramid: LL0 + 3 detail levels (coarse->fine).
// out_y0 = per-level output y0; vlow_even = (out_y0 % 2 == 0).
Grid reconstructPyramid(const Grid& LL0, const std::array<Grid, 4>& d1,
                        const std::array<Grid, 4>& d2,
                        const std::array<Grid, 4>& d3,
                        const std::array<int, 3>& outY0) {
  Grid cur = LL0;
  const std::array<const std::array<Grid, 4>*, 3> details = {&d1, &d2, &d3};
  for (int i = 0; i < 3; ++i) {
    const auto& d = *details[i];
    bool ve = (outY0[i] % 2 == 0);
    cur = inv53Level(cur, d[S28], d[S30], d[S38], ve);
  }
  return cur;
}

// ------------------------------------------------------------------------- //
// Luma post-filter: a vertical-5/3 ring combined with a horizontal-5/3 pass.
// ------------------------------------------------------------------------- //

inline int64_t u16v(int64_t v) { return static_cast<uint16_t>(v); }

// The PostFilter vertical-ring out0 line (the LOW vertical band).
void pfVpredict(const std::vector<int64_t>& s0,
                const std::vector<int64_t>& above,
                const std::vector<int64_t>& below, int w,
                std::vector<int64_t>& out0) {
  out0.assign(w, 0);
  for (int i = 0; i < w; ++i) {
    int64_t ab0 = wrapI16(above[i]);
    int64_t bl0 = wrapI16(below[i]);
    int64_t ab1 = wrapI16(above[std::min(i + 1, w - 1)]);
    int64_t bl1 = wrapI16(below[std::min(i + 1, w - 1)]);
    int64_t res;
    if (i == w - 1) {
      uint32_t last = static_cast<uint32_t>(ab0 + bl0);
      res = (static_cast<int64_t>(
                 static_cast<uint32_t>(2 * u16v(s0[i]) - (last >> 1))) >>
             1);
    } else {
      uint32_t summ = static_cast<uint32_t>(ab0 + ab1 + bl0 + bl1);
      res = (static_cast<int64_t>(
                 static_cast<uint32_t>(2 * u16v(s0[i]) - (summ >> 2))) >>
             1);
    }
    out0[i] = wrapI16(res);
  }
}

// The PostFilter horizontal-5/3 write: interleave LOW (odd cols) + predicted.
void pfHwrite(const std::vector<int64_t>& LO, const std::vector<int64_t>& HI,
              const std::vector<int64_t>& LOX, int w,
              std::vector<int64_t>& res) {
  res.assign(static_cast<size_t>(2) * w, 0);
  uint32_t w11 = static_cast<uint32_t>(wrapI16(LO[0]) + wrapI16(LOX[0]));
  res[0] = wrapI16(static_cast<int64_t>(u16v(HI[0])) + (w11 >> 1));
  res[1] = wrapI16(wrapI16(LO[0]));
  int64_t prev = wrapI16(LO[0]);
  for (int i = 1; i < w; ++i) {
    uint32_t acc = static_cast<uint32_t>(wrapI16(LO[i]) + prev +
                                         wrapI16(LOX[i]) + wrapI16(LOX[i - 1]));
    res[2 * i] = wrapI16(static_cast<int64_t>(u16v(HI[i])) + (acc >> 2));
    res[2 * i + 1] = wrapI16(wrapI16(LO[i]));
    prev = wrapI16(LO[i]);
  }
}

// One ring line: lo + hi bands.
struct RingLine {
  std::vector<int64_t> lo;
  std::vector<int64_t> hi;
  bool valid = false;
};

// Run the streaming luma PostFilter over a column slice [xLo, xHi) of the two
// sub-wavelet planes (s0 = Y-detail recon, s1 = Y-LL grid). Produces a
// (H x 2*(xHi-xLo)) int16 luma-coeff plane (the log->linear map's source).
Grid postfilterPlane(const Grid& s0Plane, const Grid& s1Plane, int xLo, int xHi,
                     int y1, int margin = 8) {
  int H = s0Plane.h;
  int ncol = xHi - xLo;
  int w = ncol + margin;
  int COMPW = s0Plane.w;

  auto srow = [&](const Grid& plane, int r, std::vector<int64_t>& seg) {
    seg.assign(w, 0);
    int64_t lastv = 0;
    for (int i = 0; i < w; ++i) {
      int col = xLo + i;
      if (col < COMPW) {
        lastv = wrapI16(plane.at(r, col));
        seg[i] = lastv;
      } else {
        seg[i] = lastv; // clamp to last available column
      }
    }
  };

  Grid outPlane(2 * ncol, H);
  std::array<RingLine, 2> ring;
  std::vector<int64_t> s0, s1, above, zeros(w, 0);
  std::vector<int64_t> loTmp, line;
  for (int cur = 1; cur <= H + 1; ++cur) {
    int r = cur - 1;
    int cm1 = cur - 1;
    int cm2 = cur - 2;
    if (cur <= y1 && r < H) {
      srow(s0Plane, r, s0);
      srow(s1Plane, r, s1);
      if (0 == cm1) {
        above = s1;
      } else {
        const RingLine& rl = ring[cm2 & 1];
        above = rl.valid ? rl.hi : zeros;
      }
      pfVpredict(s0, above, s1, w, loTmp);
      ring[cm1 & 1].lo = loTmp;
      ring[cm1 & 1].hi = s1;
      ring[cm1 & 1].valid = true;
    }
    if (cur >= 2 && ring[cm2 & 1].valid) {
      const RingLine& r2 = ring[cm2 & 1];
      const std::vector<int64_t>* lox;
      if (y1 == cm1) {
        lox = &r2.lo;
      } else {
        lox = ring[cm1 & 1].valid ? &ring[cm1 & 1].lo : &r2.lo;
      }
      pfHwrite(r2.lo, r2.hi, *lox, w, line);
      int orow = cur - 2;
      if (orow >= 0 && orow < H) {
        for (int x = 0; x < 2 * ncol; ++x)
          outPlane.at(orow, x) = wrapI16(line[x]);
      }
    }
  }
  return outPlane;
}

// Combine the two side-by-side base wavelet planes into the full-width
// luma-coeff plane.
Grid reconstructLumaCoeff(const Grid& Ydet, const Grid& Yll, int totalW,
                          int half) {
  int COMPW = Ydet.w;
  int H = Ydet.h;
  int seam = 2 * half;
  Grid pf0 = postfilterPlane(Ydet, Yll, 0, half + 8, /*y1=*/H);
  Grid pf1 = postfilterPlane(Ydet, Yll, half, COMPW, /*y1=*/H);
  Grid luma(totalW, H);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x <= seam && x < totalW; ++x)
      luma.at(y, x) = pf0.at(y, x);
    // pf1 local col k -> output col seam+k, starting at local col 1.
    for (int x = seam + 1; x < totalW; ++x) {
      int local = x - seam; // 1, 2, ...
      if (local < pf1.w)
        luma.at(y, x) = pf1.at(y, local);
    }
  }
  return luma;
}

// ------------------------------------------------------------------------- //
// Per-tile reconstruction (region buffer -> RGGB CFA block).
// ------------------------------------------------------------------------- //

// Decode a visual component's full subband pyramid (LL0 + 3 detail levels).
// ci: 0=Y-detail, 1=Cb, 2=Cr.
Grid decodeComponentPyramid(CompBytes comp,
                            const std::array<PicRecord, 5>& recs, int ci,
                            const std::array<LevelGeom, 4>& geom,
                            int canvasOrigin) {
  std::array<std::array<Grid, 4>, 4> g;
  for (int lvl = 0; lvl < 4; ++lvl) {
    if (ci >= recs[lvl].count)
      ThrowRDE("ARW6: missing component %d in record %d", ci, lvl);
    decodeResolution(comp, recs[lvl].offsets[ci], lvl,
                     /*isLl=*/0, geom[lvl], g[lvl]);
  }
  // Vertical inter-level parity origins for the 3 inverse-5/3 combine steps:
  // the output origin of each step = {res2.y0, res3.y0, canvasOrigin}
  // (= {3,5,10} for FF, {2,4,7} for APS-C). Derived from the geometry so it
  // generalises across tile heights.
  return reconstructPyramid(
      g[0][S20], g[1], g[2], g[3],
      {geom[2].g[S20].y0, geom[3].g[S20].y0, canvasOrigin});
}

// Log->linear map: lin = clamp(coeff + 2048, 0, 4095); Y = LUT[lin]. Fills
// `lin` (the shared grayscale reference) and `Yp` (the final luma plane).
void setLineLogLuma(const Grid& luma, Grid& lin, Plane& Yp) {
  int H = luma.h;
  int PW = luma.w;
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < PW; ++x) {
      int v = std::clamp(static_cast<int>(luma.at(y, x)) + 2048, 0, 4095);
      lin.at(y, x) = static_cast<int16_t>(v);
      Yp.at(y, x) = SonyArw6LogToLinear[v];
    }
  }
}

// Chroma colour-conversion against the luma reference:
// plane = LUT[clamp(((lin[2i]+lin[2i+1])>>1) + 2*coeff, 0, 4095)].
Plane colorConvChroma(const Grid& cf, const Grid& lin, int COMPW) {
  int H = lin.h;
  Plane plane(COMPW, H, Plane::NoInit{}); // every cell written below
  for (int y = 0; y < H; ++y) {
    for (int i = 0; i < COMPW; ++i) {
      int g = lin.at(y, 2 * i);
      int h = lin.at(y, (2 * i) + 1);
      int cval = (y < cf.h && i < cf.w) ? static_cast<int>(cf.at(y, i)) : 0;
      int v = std::clamp(((g + h) >> 1) + (2 * cval), 0, 4095);
      plane.at(y, i) = SonyArw6LogToLinear[v];
    }
  }
  return plane;
}

// Interleave Y/Cb/Cr into the RGGB CFA, fused with tile placement:
//   block[2k  , 2i  ] = Cb[k, i]                   (R site)
//   block[2k  , 2i+1] = Y [1 + 2i] of plane row k  (G)
//   block[2k+1, 2i  ] = Y [    2i] of plane row k  (G)
//   block[2k+1, 2i+1] = Cr[k, i]                   (B site)
// and write the (2*BR x PW) block straight into the output at (x0, y0), clipped
// to the active (clipW x clipH) frame. Writing uint16 directly avoids
// materialising a full int32 CFA grid per tile and the separate serial copy.
void composeTileToOutput(Array2DRef<uint16_t> out, const Plane& Yp,
                         const Plane& Cb, const Plane& Cr, int PW, int BR,
                         int x0, int y0, int clipW, int clipH) {
  int halfW = PW / 2;
  for (int k = 0; k < BR; ++k) {
    const int oy0 = y0 + (2 * k);
    const int oy1 = oy0 + 1;
    if (oy0 >= clipH)
      break; // all remaining rows are below the frame
    const bool r1 = oy1 < clipH;
    for (int i = 0; i < halfW; ++i) {
      const int ox0 = x0 + (2 * i);
      const int ox1 = ox0 + 1;
      if (ox0 >= clipW)
        break; // all remaining columns are right of the frame
      const bool c1 = ox1 < clipW;
      const int yEvenCol = (2 * i) + 1; // odd column, output row 2k
      const int yOddCol = 2 * i;        // even column, output row 2k+1
      out(oy0, ox0) =
          static_cast<uint16_t>((k < Cb.h && i < Cb.w) ? Cb.at(k, i) : 0);
      if (c1)
        out(oy0, ox1) = static_cast<uint16_t>(
            (k < Yp.h && yEvenCol < Yp.w) ? Yp.at(k, yEvenCol) : 0);
      if (r1)
        out(oy1, ox0) = static_cast<uint16_t>(
            (k < Yp.h && yOddCol < Yp.w) ? Yp.at(k, yOddCol) : 0);
      if (r1 && c1)
        out(oy1, ox1) =
            static_cast<uint16_t>((k < Cr.h && i < Cr.w) ? Cr.at(k, i) : 0);
    }
  }
}

// Per-tile geometry + placement, parsed once (cheaply, no entropy decode) from
// the tile sub-header and picture header. Shared read-only by the (independent)
// component decodes, so it is split out from the heavy reconstruction.
struct TileCtx {
  const uint8_t* region = nullptr;
  int regionLen = 0;
  int PW = 0;    // tile width
  int BR = 0;    // Bayer-row count (tile height = 2*BR)
  int COMPW = 0; // per-component width (PW/2)
  int Wb = 0;    // LL0 width
  int half = 0;  // base sub-wavelet width
  int a = 0;     // canvas origin
  std::array<PicRecord, 5> recs{};
  std::array<LevelGeom, 4> geom{};
  int x0 = 0; // output placement
  int y0 = 0;
};

// The (up to) four independent per-tile component decodes.
enum TileComp { COMP_YDET = 0, COMP_YLL = 1, COMP_CB = 2, COMP_CR = 3 };

TileCtx parseTileCtx(const uint8_t* region, int regionLen) {
  // Tile sub-header: a 1-byte format tag + "000", u32 index, u16 segW (=PW),
  // u16 segH/2 (=BR), ... The tag byte varies by body ('0' on the A7R VI, 'A'
  // on the A7 V); only the trailing "000" is the fixed marker. The geometry and
  // picture-header validation below reject anything that slips past it.
  if (region[1] != '0' || region[2] != '0' || region[3] != '0')
    ThrowRDE("ARW6: bad tile sub-header marker");
  int PW = (region[8] << 8) | region[9];
  int BR = (region[10] << 8) | region[11];
  // BR must be even: arw6CanvasOrigin has no solution otherwise, and every real
  // frame uses an even Bayer-row count.
  if (PW <= 0 || BR <= 0 || (PW & 1) != 0 || (BR & 1) != 0 || PW > 10240 ||
      BR > 4096)
    ThrowRDE("ARW6: bad tile geometry (%d x %d)", PW, 2 * BR);

  TileCtx c;
  c.region = region;
  c.regionLen = regionLen;
  c.PW = PW;
  c.BR = BR;
  c.COMPW = PW / 2;
  c.Wb = (c.COMPW + 7) / 8;
  c.half = c.COMPW / 2;
  c.a = arw6CanvasOrigin(BR);
  c.recs = parsePictureHeader(region, regionLen);
  c.geom = chromaGeometry(c.Wb, BR);
  return c;
}

// Decode one independent component for a tile. COMP_YLL returns the raw is_ll
// grid (resized against the Y-detail height later, in finishTile); the others
// return the reconstructed component pyramid.
Grid decodeTileComponent(const TileCtx& c, int comp) {
  CompBytes cb{c.region + 0x80, c.regionLen - 0x80};
  if (comp == COMP_YLL) {
    std::array<LevelGeom, 4> yllGeom{};
    yllGeom[0].g[S20] = {c.COMPW, c.a, c.a + (2 * c.BR), 4};
    yllGeom[0].active[S20] = true;
    std::array<Grid, 4> yllGrids;
    if (c.recs[4].count < 1)
      ThrowRDE("ARW6: missing Y-LL record");
    decodeResolution(cb, c.recs[4].offsets[0], /*level=*/0,
                     /*isLl=*/1, yllGeom[0], yllGrids);
    return std::move(yllGrids[S20]);
  }
  int ci = (comp == COMP_YDET) ? 0 : (comp == COMP_CB ? 1 : 2);
  return decodeComponentPyramid(cb, c.recs, ci, c.geom, c.a);
}

// Combine a tile's decoded components (Y-detail pyramid, raw Y-LL, Cb/Cr coeff
// pyramids) into the RGGB CFA and write it into the output at the tile's
// placement, clipped to the active frame.
void finishTile(const TileCtx& c, const Grid& Ydet, const Grid& YllRaw,
                const Grid& CbCoef, const Grid& CrCoef,
                Array2DRef<uint16_t> out, int clipW, int clipH) {
  int H = Ydet.h;

  // Y-LL: resize the raw is_ll grid to the luma height.
  Grid Yll(c.COMPW, H);
  for (int y = 0; y < H && y < YllRaw.h; ++y)
    for (int x = 0; x < c.COMPW; ++x)
      Yll.at(y, x) = YllRaw.at(y, x);

  // Luma: combine the two side-by-side base planes via the post-filter, then
  // the log->linear map.
  Grid luma = reconstructLumaCoeff(Ydet, Yll, c.PW, c.half);
  Grid lin(c.PW, H, Grid::NoInit{}); // every cell written by setLineLogLuma
  Plane Yp(c.PW, H, Plane::NoInit{});
  setLineLogLuma(luma, lin, Yp);

  // Chroma: colour-conv each coeff pyramid against the luma reference.
  Plane Cb = colorConvChroma(CbCoef, lin, c.COMPW);
  Plane Cr = colorConvChroma(CrCoef, lin, c.COMPW);

  composeTileToOutput(out, Yp, Cb, Cr, c.PW, c.BR, c.x0, c.y0, clipW, clipH);
}

} // namespace

// ------------------------------------------------------------------------- //
// Per-tile reconstruction + compose into the CFA mosaic.
// ------------------------------------------------------------------------- //

SonyArw6Decompressor::SonyArw6Decompressor(RawImage img, ByteStream input_,
                                           int width_, int height_)
    : mRaw(std::move(img)), input(input_), width(width_), height(height_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");
  if (width <= 0 || height <= 0 || width > 10240 || height > 7168)
    ThrowRDE("Unexpected image dimensions: (%d; %d)", width, height);
}

void SonyArw6Decompressor::decompress() const {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  // Parse the strip header: u32[0]=nseg, [2]=512 (data start), then nseg
  // records of 6 u32 {w, h, end, f3, px, py}.
  ByteStream hdr = input;
  hdr.setByteOrder(Endianness::little);
  uint32_t nseg = hdr.peekU32(0);
  if (nseg < 1 || nseg > 4)
    ThrowRDE("ARW6: unexpected tile count %u", nseg);

  // The container records start at u32 index 6. Compute per-tile byte ranges:
  // record[i].end is the strip offset where tile i's data ends; tile 0 starts
  // at 512. (record end==0 for the last tile -> ends at strip end.)
  std::array<uint32_t, 4> tileEnd{};
  for (uint32_t i = 0; i < nseg; ++i)
    tileEnd[i] = hdr.peekU32(6 + 6 * i + 2);

  uint32_t stripLen = input.getSize();
  std::array<uint32_t, 4> tileStart{};
  std::array<uint32_t, 4> tileStop{};
  tileStart[0] = 512;
  for (uint32_t i = 0; i < nseg; ++i) {
    tileStop[i] = (tileEnd[i] != 0) ? tileEnd[i] : stripLen;
    if (i + 1 < nseg)
      tileStart[i + 1] = tileStop[i];
  }

  // Each spatial tile is placed on the 2x2 grid by strip index (column = s/2,
  // row = s%2). Parse every tile's headers up front (serially, cheaply) to
  // validate its byte range/geometry and resolve placements: the left column's
  // tile width sets the column-1 x-offset, and each tile's own height (2*BR)
  // sets the row-1 y-offset (matching the historical placement).
  std::array<TileCtx, 4> ctx{};
  for (uint32_t s = 0; s < nseg; ++s) {
    if (tileStart[s] >= stripLen || tileStop[s] > stripLen ||
        tileStop[s] <= tileStart[s])
      ThrowRDE("ARW6: bad tile byte range");
    Buffer regionBuf = input.peekBuffer(stripLen).getSubView(
        tileStart[s], tileStop[s] - tileStart[s]);
    int regionLen = implicit_cast<int>(regionBuf.getSize());
    if (regionLen < 0x80 + 16)
      ThrowRDE("ARW6: tile region too small");
    ctx[s] = parseTileCtx(regionBuf.begin(), regionLen);
  }
  int leftColWidth = 0;
  for (uint32_t s = 0; s < nseg; ++s) {
    if (s / 2 == 0)
      leftColWidth = ctx[s].PW;
  }
  for (uint32_t s = 0; s < nseg; ++s) {
    int col = static_cast<int>(s) / 2;
    int rowi = static_cast<int>(s) % 2;
    ctx[s].x0 = col * (leftColWidth != 0 ? leftColWidth : ctx[s].PW);
    ctx[s].y0 = rowi * (2 * ctx[s].BR);
    // Every tile must fit inside the declared frame. This ties the (otherwise
    // independent) per-tile PW/BR caps to the image dimensions, so a few header
    // bytes cannot request reconstruction work far exceeding width*height
    // (a decode bomb), and keeps the per-tile output rectangles disjoint.
    if (ctx[s].x0 + ctx[s].PW > width || ctx[s].y0 + (2 * ctx[s].BR) > height)
      ThrowRDE("ARW6: tile %u geometry (%d+%d, %d+%d) exceeds frame (%d, %d)",
               s, ctx[s].x0, ctx[s].PW, ctx[s].y0, 2 * ctx[s].BR, width,
               height);
  }

  // Phase A: decode every tile's (up to) four independent components.
  // Flattening tiles x components into one work-list lets the thread pool fill
  // past the tile count (e.g. 16 units for a full frame, 4 for a single APS-C
  // tile). Each unit owns a distinct comps[tile][component] slot. (No-op
  // without OpenMP.) Exceptions cannot cross the OpenMP region, so they are
  // funnelled through the RawImage error log and re-raised serially after each
  // phase, per the rawspeed idiom.
  std::vector<std::array<Grid, 4>> comps(nseg);
  const auto nsegI = implicit_cast<int>(nseg);
  const int nUnits = nsegI * 4;
  const int nThreads = rawspeed_get_number_of_processor_cores();
#ifdef HAVE_OPENMP
#pragma omp parallel for schedule(dynamic) num_threads(nThreads)
#endif
  for (int u = 0; u < nUnits; ++u) {
    try {
      comps[u / 4][u % 4] = decodeTileComponent(ctx[u / 4], u % 4);
    } catch (const RawspeedException& e) {
      mRaw->setError(e.what());
    } catch (...) {
      // Nothing (e.g. std::bad_alloc from the per-unit allocations) may cross
      // the OpenMP region boundary; funnel it like any other decode error.
      mRaw->setError("ARW6: component decode failed");
    }
  }

  std::string firstErr;
  if (mRaw->isTooManyErrors(1, &firstErr))
    ThrowRDE("ARW6: tile component decode failed: %s", firstErr.c_str());

    // Phase B: per tile, combine its components (luma PostFilter + colour-conv)
    // and write the RGGB block into its disjoint, clipped output rectangle.
#ifdef HAVE_OPENMP
#pragma omp parallel for schedule(static) num_threads(nThreads)
#endif
  for (int s = 0; s < nsegI; ++s) {
    try {
      finishTile(ctx[s], comps[s][COMP_YDET], comps[s][COMP_YLL],
                 comps[s][COMP_CB], comps[s][COMP_CR], out, width, height);
    } catch (const RawspeedException& e) {
      mRaw->setError(e.what());
    } catch (...) {
      mRaw->setError("ARW6: tile finish failed");
    }
  }

  if (mRaw->isTooManyErrors(1, &firstErr))
    ThrowRDE("ARW6: tile finish failed: %s", firstErr.c_str());
}

} // namespace rawspeed
