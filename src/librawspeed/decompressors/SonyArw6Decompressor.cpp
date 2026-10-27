/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2026 Nikolay Amiantov

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

#include "rawspeedconfig.h"
#include "decompressors/SonyArw6Decompressor.h"
#include "adt/Array1DRef.h"
#include "adt/Array2DRef.h"
#include "adt/Bit.h"
#include "adt/Casts.h"
#include "adt/CroppedArray2DRef.h"
#include "adt/DefaultInitAllocatorAdaptor.h"
#include "adt/Invariant.h"
#include "adt/Point.h"
#include "bitstreams/BitStreamerMSB.h"
#include "common/Common.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rawspeed {

namespace {

// Sizes in the container are measured in 16-byte blocks.
constexpr int BlockBytes = 16;

// A tile always carries five coding regions: LL, three detail levels
// (coarsest first), green_hi.
constexpr int NumRegions = 5;
constexpr int NumComponents = 3; // green_lo, chroma_r, chroma_b
// Vertical levels on the shared canvas. Chunks are sized so that the
// coarsest level has one lattice row per chunk, so the chunk height equals
// the coarsest level's step; the chunks are anchored at the coarsest detail
// lattice's first row (half of that step).
constexpr int NumLevels = NumRegions - 1;
constexpr int ChunkHeight = 1 << (NumLevels - 1);
constexpr int ChunkOrigin = 1 << (NumLevels - 2);

// Decoded sample precision of the codec. Greens are stored offset by
// MidValue; everything clamps to [0, MaxValue] when assembled.
constexpr int BitDepth = 12;
constexpr int MidValue = 1 << (BitDepth - 1);
constexpr int MaxValue = (1 << BitDepth) - 1;

// Coefficients are entropy-coded in fixed-size groups sharing one Rice
// parameter k.
constexpr int GroupSize = 4;

// ---------------- parsed tile structures ----------------

struct Chunk final {
  uint16_t length;          // byte length (0 = empty chunk)
  std::array<uint8_t, 3> q; // per-orientation quantizers
};

// One component's data within one coding region.
struct ComponentData final {
  std::vector<Chunk> chunks; // the component's chunks, top to bottom
  ByteStream data;           // the entropy-coded chunks, concatenated
};

// ---------------- tile geometry ----------------
// The codec places every region's rows on one shared 1-D canvas to determine
// which rows are stored in which chunks. Each orientation uses a lattice - a
// strided subset of the canvas rows - and each chunk covers ChunkHeight
// canvas rows. The canvas is anchored so that the coarsest level's detail
// lattice starts at ChunkOrigin, and the components' rows start at compTop,
// chosen to make the first and last chunks cover the same amount of data.
// None of this is stored in the file, so it is derived here.

// A span of a lattice: *lattice* rows [top, bottom) hold data; chunk 0's
// window begins at lattice row rowBase.
struct LatticeSpan final {
  int top;
  int bottom;
  int rowBase;
};

// One coding region's geometry, shared by every component within it.
struct RegionGeometry final {
  int width;
  // Lattice rows of this region stored per chunk (= ChunkHeight / the
  // lattice's canvas step).
  int rowsPerChunk;
  int nOrientations; // 1 for LL / green_hi, 3 for detail (HL, LH, HH)
  std::array<LatticeSpan, 3> orientations;
  int nComponents; // 3 for LL / detail regions, 1 for green_hi
  // Vertical polyphase flip for recombining a detail region: set when the
  // recombined plane's first row comes from the odd lattice.
  bool vflip;
};

std::array<RegionGeometry, NumRegions> deriveGeometry(int tileW, int tileH) {
  invariant(tileW % 2 == 0 && tileH % 2 == 0);
  const int compHeight = tileH / 2; // every component is half the tile tall
  // The offset past ChunkOrigin that makes the first and last chunks
  // symmetric.
  const int compTop =
      ChunkOrigin +
      (ChunkHeight - (compHeight / 2) % ChunkHeight) % ChunkHeight;
  const int compBottom = compTop + compHeight;

  struct LevelGeom final {
    int width;
    int step;
    LatticeSpan even; // the lattice at canvas offset 0
    LatticeSpan odd;  // the lattice offset by half a step
  };
  // Level 1 is the finest (step 1), level NumLevels the coarsest.
  std::array<LevelGeom, 1 + NumLevels> levels;
  for (int level = 1; level <= NumLevels; ++level) {
    const int step = 1 << (level - 1);
    auto span = [compTop, compBottom, step](int phase) {
      invariant(compTop >= phase);
      return LatticeSpan{
          implicit_cast<int>(roundUpDivision(compTop - phase, step)),
          implicit_cast<int>(roundUpDivision(compBottom - phase, step)),
          implicit_cast<int>(roundUpDivision(ChunkOrigin - phase, step))};
    };
    levels[level] = {tileW >> level, step, span(0), span(step / 2)};
  }

  // The vertical polyphase flip of a detail level: the parity of its
  // recombined plane's top coordinate on the half-step lattice.
  auto vflip = [compTop](int step) {
    return (roundUpDivision(compTop, step >> 1) & 1) != 0;
  };

  std::array<RegionGeometry, NumRegions> geom;
  // Region 0: LL, on the coarsest lattice, one orientation.
  const LevelGeom& coarsest = levels[NumLevels];
  geom[0] = {coarsest.width,
             ChunkHeight / coarsest.step,
             1,
             {coarsest.even},
             NumComponents,
             false};
  // Regions 1..NumLevels-1: the 2-D detail levels, coarsest first. HL is on
  // the even lattice, LH and HH share the odd one.
  for (int region = 1; region < NumLevels; ++region) {
    const LevelGeom& lvl = levels[NumLevels + 1 - region];
    geom[region] = {
        lvl.width,     ChunkHeight / lvl.step, 3, {lvl.even, lvl.odd, lvl.odd},
        NumComponents, vflip(lvl.step)};
  }
  // Region NumLevels: green_hi, on the finest lattice, a single component.
  const LevelGeom& finest = levels[1];
  geom[NumLevels] = {
      finest.width, ChunkHeight / finest.step, 1, {finest.even}, 1, false};
  return geom;
}

// ---------------- entropy decoding ----------------

// Read a unary run of zeros terminated by a one.
int readUnaryZeros(BitStreamerMSB& bs) {
  int count = 0;
  while (bs.getBits(1) == 0)
    ++count;
  return count;
}

// Like readUnaryZeros, but once `max` zeros have been read stop without
// consuming a terminator.
int readUnaryZerosCapped(BitStreamerMSB& bs, int max) {
  int count = 0;
  while (count < max && bs.getBits(1) == 0)
    ++count;
  return count;
}

// Adaptive update of the Rice parameter k, read as a variable-length code:
// '0' keeps k; '10' + (run of zeros, terminated by 1) raises k by 1+run;
// '11' + (run of zeros) lowers k by 1+run.
int updateK(BitStreamerMSB& bs, int k) {
  if (bs.getBits(1) == 0)
    return k;
  if (bs.getBits(1) == 0)
    return k + 1 + readUnaryZeros(bs);
  // k is a non-negative Rice parameter, so a decrease at k <= 0 is an
  // impossible state.
  if (k <= 0)
    ThrowRDE("Decrease of Rice parameter k below 0");
  k -= 1;
  // The decrease is a unary run of zeros, but once k hits 0 no terminating
  // one is emitted.
  return k - readUnaryZerosCapped(bs, k);
}

// Decode one group of coefficients into out[0..GroupSize) and return the
// next k. Each coefficient is a k-bit absolute value; then k is updated
// (unless this is the line's last group), then one sign bit per non-zero
// coefficient is read.
int decodeGroup(BitStreamerMSB& bs, int k, bool notLast,
                Array1DRef<int32_t> out) {
  invariant(k > 0);
  invariant(out.size() == GroupSize);
  if (k > BitStreamerMSB::Cache::MaxGetBits)
    ThrowRDE("Rice parameter k exceeds the bit reader width");
  for (int i = 0; i != GroupSize; ++i)
    out(i) = implicit_cast<int32_t>(bs.getBits(k));
  const int newK = notLast ? updateK(bs, k) : k;
  for (int i = 0; i != GroupSize; ++i) {
    if (out(i) != 0 && bs.getBits(1) != 0)
      out(i) = -out(i);
  }
  return newK;
}

// Decode one coefficient line of four-coefficient groups filling all of
// `out` (whose size must be a multiple of GroupSize). k is stored first.
// While k == 0 the line is coded as runs of all-zero groups; otherwise each
// group is decodeGroup-coded.
void decodeLine(BitStreamerMSB& bs, Array1DRef<int32_t> out) {
  invariant(out.size() % GroupSize == 0);
  const int groups = out.size() / GroupSize;
  std::fill(out.begin(), out.end(), 0);
  int k = updateK(bs, 0);
  int group = 0;
  while (group < groups) {
    if (k == 0) {
      const int remaining = groups - group;
      if (remaining == 1) {
        // A lone trailing group is implicitly zero; nothing more to read.
        break;
      }
      // Elias-gamma-style coding for the number of zero groups. First, read
      // the exponent part.
      const int exponent =
          readUnaryZerosCapped(bs, std::bit_width(unsigned(remaining - 1)));
      int run = 1 << exponent;
      // If the run already covers the remaining groups, end the line.
      if (run >= remaining)
        break;
      if (exponent > 0)
        run += implicit_cast<int>(bs.getBits(exponent));
      if (run > remaining)
        ThrowRDE("Zero-run overruns the line");
      group += run;
      // Then k climbs.
      k = 1 + readUnaryZeros(bs);
    } else {
      k = decodeGroup(bs, k, group < groups - 1,
                      out.getBlock(GroupSize, group).getAsArray1DRef());
      ++group;
    }
  }
}

// Mid-rise dequantizer, reconstructing a magnitude to its bin centroid
// (mag + 0.5) * 2^q - 0.5. The trailing -0.5 is approximated as -(mag & 1):
// rounded down on odd magnitudes and up on even ones. q == 0 is identity,
// zero values are identity too, signs are preserved.
int16_t dequant(int32_t value, int q) {
  invariant(q >= 0);
  if (value == 0 || q == 0)
    return static_cast<int16_t>(value);
  const int64_t mag = std::abs(int64_t(value));
  const int64_t recon = (((mag << 1) + 1) << (q - 1)) - (mag & 1);
  return static_cast<int16_t>(value < 0 ? -recon : recon);
}

// ---------------- image planes ----------------

class Plane final {
  std::vector<int16_t, DefaultInitAllocatorAdaptor<int16_t>> storage;
  Array2DRef<int16_t> ref;

public:
  Plane(int width, int height)
      : ref(Array2DRef<int16_t>::create(storage, width, height)) {}

  Plane(const Plane&) = delete;
  Plane& operator=(const Plane&) = delete;
  Plane(Plane&&) = default;
  Plane& operator=(Plane&&) = default;

  void fill(int16_t value) { std::fill(storage.begin(), storage.end(), value); }

  Array2DRef<int16_t> view() { return ref; }
  Array2DRef<const int16_t> view() const { return ref; }
};

int clampIndex(int i, int n) {
  invariant(n > 0);
  return std::clamp(i, 0, n - 1);
}

// Inverse horizontal DPCM in place over each row of the LL sub-band.
void predictHorizontal(Array2DRef<int16_t> plane) {
  for (int r = 0; r != plane.height(); ++r) {
    Array1DRef<int16_t> row = plane[r];
    int acc = row(0);
    for (int i = 1; i != plane.width(); ++i) {
      acc += row(i);
      row(i) = static_cast<int16_t>(acc);
    }
  }
}

// ---------------- the inverse LeGall 5/3 wavelet ----------------

// One vertical inverse 5/3 lift: interleave the coarse (`low`) and detail
// (`high`) sub-bands back onto a single grid. `flip` selects if the coarse
// sub-band lands on the even output rows or on the odd ones.
Plane idwt53Vertical(Array2DRef<const int16_t> low,
                     Array2DRef<const int16_t> high, bool flip) {
  invariant(low.width() == high.width());
  const int nLow = low.height();
  const int nHigh = high.height();
  const int w = low.width();
  Plane out(w, nLow + nHigh);
  out.fill(0);
  const Array2DRef<int16_t> dst = out.view();
  const int c = flip ? 1 : 0; // coarse-row output parity; detail takes 1 - c
  // Undo the update: coarse from its two flanking details.
  for (int n = 0; n != nLow; ++n) {
    const int pos = 2 * n + c;
    if (pos >= dst.height()) // a trailing row past the grid edge
      continue;
    Array1DRef<const int16_t> h0 = high[clampIndex(n - 1 + c, nHigh)];
    Array1DRef<const int16_t> h1 = high[clampIndex(n + c, nHigh)];
    Array1DRef<const int16_t> l = low[n];
    Array1DRef<int16_t> o = dst[pos];
    for (int i = 0; i != w; ++i)
      o(i) = static_cast<int16_t>(l(i) - ((h0(i) + h1(i) + 2) >> 2));
  }
  // Undo the predict: detail from its two flanking coarses.
  for (int n = 0; n != nHigh; ++n) {
    const int pos = 2 * n + 1 - c;
    if (pos >= dst.height())
      continue;
    Array1DRef<const int16_t> e0 = dst[2 * clampIndex(n - c, nLow) + c];
    Array1DRef<const int16_t> e1 = dst[2 * clampIndex(n + 1 - c, nLow) + c];
    Array1DRef<const int16_t> h = high[n];
    Array1DRef<int16_t> o = dst[pos];
    for (int i = 0; i != w; ++i)
      o(i) = static_cast<int16_t>(h(i) + ((e0(i) + e1(i)) >> 1));
  }
  return out;
}

// The horizontal inverse 5/3 lift on each row; the coarse samples always
// land on the even output columns.
Plane idwt53Horizontal(Array2DRef<const int16_t> left,
                       Array2DRef<const int16_t> right, int nRows, int nCols) {
  Plane out(2 * nCols, nRows);
  const Array2DRef<int16_t> dst = out.view();
  for (int r = 0; r != nRows; ++r) {
    Array1DRef<const int16_t> lo = left[r];
    Array1DRef<const int16_t> hi = right[r];
    Array1DRef<int16_t> o = dst[r];
    for (int n = 0; n != nCols; ++n) {
      o(2 * n) = static_cast<int16_t>(
          lo(n) - ((hi(clampIndex(n - 1, nCols)) + hi(n) + 2) >> 2));
    }
    for (int n = 0; n != nCols; ++n) {
      o(2 * n + 1) = static_cast<int16_t>(
          hi(n) + ((o(2 * n) + o(2 * clampIndex(n + 1, nCols))) >> 1));
    }
  }
  return out;
}

// One inverse 2-D 5/3 level: vertical passes with the given flip (column
// pairs LL|LH and HL|HH), then the horizontal pass over the common area.
Plane idwt2d(const Plane& ll, const Plane& lh, const Plane& hl, const Plane& hh,
             bool flip) {
  if (ll.view().width() != lh.view().width() ||
      hl.view().width() != hh.view().width())
    ThrowRDE("Sub-band widths do not agree");
  const Plane left = idwt53Vertical(ll.view(), lh.view(), flip);
  const Plane right = idwt53Vertical(hl.view(), hh.view(), flip);
  const int nRows = std::min(left.view().height(), right.view().height());
  const int nCols = std::min(left.view().width(), right.view().width());
  return idwt53Horizontal(left.view(), right.view(), nRows, nCols);
}

// The quincunx (diamond) green wavelet's update undone: recover the
// low-pass from green_lo (`lo`) and the four green_hi (`hi`) details around
// each sample, in this row and the one above it. Doubling before the final
// shift rounds the half away from zero. Only the first `h` rows take part.
Plane diamondLow(Array2DRef<const int16_t> lo, Array2DRef<const int16_t> hi,
                 int h) {
  invariant(lo.width() == hi.width());
  invariant(h <= lo.height() && h <= hi.height());
  const int w = lo.width();
  Plane low(w, h);
  const Array2DRef<int16_t> dst = low.view();
#ifdef HAVE_OPENMP
#pragma omp taskloop default(none) firstprivate(lo, hi, dst, w, h)             \
    num_tasks(rawspeed_get_number_of_processor_cores())
#endif
  for (int r = 0; r < h; ++r) {
    Array1DRef<const int16_t> hiR = hi[r];
    Array1DRef<const int16_t> hiR0 = hi[clampIndex(r - 1, h)];
    Array1DRef<const int16_t> loR = lo[r];
    Array1DRef<int16_t> o = dst[r];
    for (int n = 0; n != w; ++n) {
      const int n1 = clampIndex(n + 1, w);
      o(n) = static_cast<int16_t>(
          (2 * loR(n) - ((hiR(n) + hiR(n1) + hiR0(n) + hiR0(n1)) >> 2)) >> 1);
    }
  }
  return low;
}

} // namespace

// ---------------- container parsing ----------------

namespace {

// Read a big-endian 24-bit block count and return it in bytes.
uint32_t readBlockSize(ByteStream& bs) {
  uint32_t v = bs.getByte();
  v = (v << 8) | bs.getByte();
  v = (v << 8) | bs.getByte();
  return v * BlockBytes;
}

// Parse one component's header block and chunk table, leaving the
// entropy-coded chunk data in ComponentData::data.
ComponentData parseComponent(ByteStream buf, int nOrientations) {
  ByteStream header = buf.getStream(BlockBytes);
  header.setByteOrder(Endianness::big);
  const int tableBlocks = header.getU16(); // chunk-table size in blocks
  uint32_t chunkBytesDeclared = readBlockSize(header); // chunk data size
  header.skipBytes(1);                                 // unknown (0x40)
  // Orientation count, in the two high bits of its byte; the rest of the
  // byte is unknown (0).
  const int orientCount = header.getByte() >> 6;
  const int chunkCount = header.getU16();
  // The remainder of the block: unknown (0x10), then padding.

  if (orientCount != nOrientations) {
    ThrowRDE("Component declares %i orientations, region has %i", orientCount,
             nOrientations);
  }

  // Cross-check the declared chunk-table size: the header block plus the
  // packed table, padded to whole blocks.
  const auto tableBytes = implicit_cast<uint32_t>(
      roundUpDivision(uint64_t(chunkCount) * (16 + 4 * orientCount),
                      8 * BlockBytes) *
      BlockBytes);
  if (tableBytes != uint32_t(tableBlocks) * BlockBytes)
    ThrowRDE("Chunk table size mismatch");

  ComponentData comp;
  comp.chunks.reserve(chunkCount);
  uint64_t chunkBytes = 0;
  if (chunkCount > 0) {
    ByteStream tableStream = buf.getStream(tableBytes);
    BitStreamerMSB table(tableStream.peekRemainingBuffer().getAsArray1DRef());
    for (int i = 0; i != chunkCount; ++i) {
      Chunk chunk = {};
      chunk.length = implicit_cast<uint16_t>(table.getBits(16));
      for (int o = 0; o != orientCount; ++o)
        chunk.q[o] = implicit_cast<uint8_t>(table.getBits(4));
      chunkBytes += chunk.length;
      comp.chunks.emplace_back(chunk);
    }
  }
  if (chunkBytesDeclared !=
      roundUpDivision(chunkBytes, BlockBytes) * BlockBytes)
    ThrowRDE("Chunk data size mismatch");

  comp.data = buf;
  return comp;
}

} // namespace

// ---------------- per-tile decoding ----------------

namespace {

// Decode one component's data within a region: all its orientations, one
// dequantized plane per orientation. For each orientation, chunk i covers
// the lattice-row window [rowBase + i*rowsPerChunk, rowBase +
// (i+1)*rowsPerChunk), capped to the span's [top, bottom).
std::vector<Plane> decodeComponent(const ComponentData& comp,
                                   const RegionGeometry& geom) {
  const int rowsPerChunk = geom.rowsPerChunk;
  const int width = geom.width;
  if (width <= 0)
    ThrowRDE("Empty sub-band");
  const int groups = implicit_cast<int>(roundUpDivision(width, GroupSize));

  std::vector<Plane> planes;
  planes.reserve(geom.nOrientations);
  for (int o = 0; o != geom.nOrientations; ++o) {
    const LatticeSpan& span = geom.orientations[o];
    if (span.bottom <= span.top)
      ThrowRDE("Empty sub-band");
    planes.emplace_back(width, span.bottom - span.top).fill(0);
  }

  std::vector<int32_t> lineStorage(size_t(GroupSize) * groups);
  const auto line = Array1DRef<int32_t>(lineStorage.data(),
                                        implicit_cast<int>(lineStorage.size()));

  ByteStream data = comp.data;
  for (int chunkIdx = 0; chunkIdx != implicit_cast<int>(comp.chunks.size());
       ++chunkIdx) {
    const Chunk& chunk = comp.chunks[chunkIdx];
    if (chunk.length == 0)
      continue;
    ByteStream chunkStream = data.getStream(chunk.length);
    auto chunkBytes = chunkStream.peekRemainingBuffer().getAsArray1DRef();
    // Tiny chunks are possible (a chunk of all-zero lines); pad them up to
    // what the bit streamer needs.
    std::array<uint8_t, BitStreamerTraits<BitStreamerMSB>::MaxProcessBytes>
        padStorage = {};
    if (chunkBytes.size() < implicit_cast<int>(padStorage.size())) {
      memcpy(padStorage.data(), chunkBytes.begin(), chunkBytes.size());
      chunkBytes = Array1DRef<const uint8_t>(
          padStorage.data(), implicit_cast<int>(padStorage.size()));
    }
    BitStreamerMSB bits(chunkBytes);

    for (int o = 0; o != geom.nOrientations; ++o) {
      const LatticeSpan& span = geom.orientations[o];
      const int q = chunk.q[o];
      const int windowTop = span.rowBase + chunkIdx * rowsPerChunk;
      // A chunk may extend out of the span; clamp it in this case.
      const int top = std::max(span.top, windowTop);
      const int bottom = std::min(span.bottom, windowTop + rowsPerChunk);
      for (int row = top; row < bottom; ++row) {
        decodeLine(bits, line);
        Array1DRef<int16_t> dst = planes[o].view()[row - span.top];
        for (int col = 0; col != width; ++col)
          dst(col) = dequant(line(col), q);
      }
    }
  }
  return planes;
}

Plane decodePlane(
    const std::array<std::vector<ComponentData>, NumRegions>& comps,
    const std::array<RegionGeometry, NumRegions>& geom, int c) {
  if (c == NumComponents) {
    std::vector<Plane> greenHi =
        decodeComponent(comps[NumLevels][0], geom[NumLevels]);
    return std::move(greenHi[0]);
  }
  // Reconstruct the three wavelet-coded components: undo the LL prediction,
  // then fold the detail levels in, coarsest first.
  std::vector<Plane> ll = decodeComponent(comps[0][c], geom[0]);
  Plane plane = std::move(ll[0]);
  predictHorizontal(plane.view());
  for (int region = 1; region != NumLevels; ++region) {
    std::vector<Plane> detail = decodeComponent(comps[region][c], geom[region]);
    // Orientation stream order is HL, LH, HH.
    plane = idwt2d(plane, detail[1], detail[0], detail[2], geom[region].vflip);
  }
  return plane;
}

} // namespace

// ---------------- the decompressor ----------------

SonyArw6Decompressor::SonyArw6Decompressor(RawImage img, ByteStream input,
                                           bool applyCurve_)
    : mRaw(std::move(img)), applyCurve(applyCurve_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != RawImageType::UINT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.hasPositiveArea() || mRaw->dim.x % 2 != 0 ||
      mRaw->dim.y % 2 != 0 || mRaw->dim.x > 10240 || mRaw->dim.y > 7168)
    ThrowRDE("Unexpected image dimensions found: (%d; %d)", mRaw->dim.x,
             mRaw->dim.y);

  input.setByteOrder(Endianness::little);
  const auto tileCount = input.getU32();
  input.skipBytes(4); // unknown
  if (tileCount == 0 || tileCount > 64)
    ThrowRDE("Unexpected tile count: %u", tileCount);

  struct TileRecord final {
    uint64_t offset;
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
  };
  std::vector<TileRecord> records;
  records.reserve(tileCount);
  for (uint32_t i = 0; i != tileCount; ++i) {
    TileRecord rec = {};
    rec.offset = input.get<uint64_t>();
    rec.x = input.getU32();
    rec.y = input.getU32();
    rec.w = input.getU32();
    rec.h = input.getU32();
    records.emplace_back(rec);
  }

  tiles.reserve(tileCount);
  for (uint32_t i = 0; i != tileCount; ++i) {
    const TileRecord& rec = records[i];
    // Each tile's payload ends where the next one begins.
    const uint64_t end =
        i + 1 != tileCount ? records[i + 1].offset : uint64_t(input.getSize());
    if (rec.offset >= end || end > uint64_t(input.getSize()))
      ThrowRDE("Tile payloads out of order or truncated");
    // The tile must lie in the mosaic, Bayer-aligned, and be big enough to
    // survive the wavelet ladder.
    if (rec.w % 2 != 0 || rec.h % 2 != 0 || rec.x % 2 != 0 || rec.y % 2 != 0 ||
        rec.w < 16 || rec.h < 16 ||
        uint64_t(rec.x) + rec.w > uint64_t(mRaw->dim.x) ||
        uint64_t(rec.y) + rec.h > uint64_t(mRaw->dim.y))
      ThrowRDE("Unexpected tile: %ux%u at (%u; %u)", rec.w, rec.h, rec.x,
               rec.y);
    TileDesc tile;
    tile.bs =
        input.getSubStream(implicit_cast<Buffer::size_type>(rec.offset),
                           implicit_cast<Buffer::size_type>(end - rec.offset));
    tile.pos = {implicit_cast<int>(rec.x), implicit_cast<int>(rec.y)};
    tile.dim = {implicit_cast<int>(rec.w), implicit_cast<int>(rec.h)};
    tiles.emplace_back(tile);
  }

  // The tiles are decoded concurrently; they must not overlap in the mosaic.
  for (size_t i = 0; i != tiles.size(); ++i) {
    for (size_t j = i + 1; j != tiles.size(); ++j) {
      const TileDesc& a = tiles[i];
      const TileDesc& b = tiles[j];
      if (iRectangle2D(a.pos, a.dim)
              .getOverlap(iRectangle2D(b.pos, b.dim))
              .hasPositiveArea())
        ThrowRDE("Tiles overlap in the mosaic");
    }
  }
}

void SonyArw6Decompressor::decompressTile(const TileDesc& t) const {
  ByteStream bs = t.bs;
  bs.setByteOrder(Endianness::big);

  // The tile sub-header block. The leading 4-byte magic is ASCII, not a
  // constant ("0000" and "A000" observed), and is not meant to be checked;
  // the next 4 bytes are unknown (sequential across a frame's tiles).
  bs.skipBytes(8);
  const int width = bs.getU16();
  const int compHeight = bs.getU16();
  // The rest of the block: 6 unknown bits (15), the decoded-sample
  // precision (16), 4 unknown bits (0), the number of colour components
  // stored (3), 1 unknown bit (0), the "mode" (3, purpose unknown), and 10
  // more unknown bits (512); then padding to the block size.
  const uint32_t packed = bs.getU32();
  const int decodedBits = implicit_cast<int>((packed >> 20) & 63);
  const int nChannels = implicit_cast<int>((packed >> 13) & 7);
  const int mode = implicit_cast<int>((packed >> 10) & 3);
  if (width != t.dim.x || compHeight != t.dim.y / 2 || decodedBits != 16 ||
      nChannels != NumComponents || mode != 3) {
    ThrowRDE("Tile sub-header mismatch: %ix%i, %i channels, %i bits, mode %i",
             width, 2 * compHeight, nChannels, decodedBits, mode);
  }

  // The region-totals area: per-region byte-size totals, two blocks.
  bs.setPosition(BlockBytes);
  std::array<uint32_t, NumRegions> totals;
  for (uint32_t& total : totals)
    total = readBlockSize(bs);

  const std::array<RegionGeometry, NumRegions> geom =
      deriveGeometry(t.dim.x, t.dim.y);

  // The per-region records: each declares its components' data sizes. A
  // record spans one block, or two from five components up.
  bs.setPosition(3 * BlockBytes);
  std::array<std::vector<uint32_t>, NumRegions> componentSizes;
  for (int region = 0; region != NumRegions; ++region) {
    const auto recordStart = bs.getPosition();
    const int count = bs.getByte() & 15; // the high nibble is unknown (0)
    if (count > 9)
      ThrowRDE("Region record declares %i components, at most 9 fit", count);
    if (count != geom[region].nComponents) {
      ThrowRDE("Region %i carries %i components, geometry expects %i", region,
               count, geom[region].nComponents);
    }
    for (int i = 0; i != count; ++i)
      componentSizes[region].emplace_back(readBlockSize(bs));
    bs.setPosition(recordStart + (count < 5 ? 1 : 2) * BlockBytes);
  }

  // The component data begins past the record blocks: each region's
  // components in turn. A region spans its declared total, which may pad
  // past its components.
  const auto dataBase = bs.getPosition();
  std::array<std::vector<ComponentData>, NumRegions> comps;
  uint64_t regionBase = 0;
  for (int region = 0; region != NumRegions; ++region) {
    uint64_t off = regionBase;
    for (const uint32_t size : componentSizes[region]) {
      ByteStream buf = bs.getSubStream(
          implicit_cast<Buffer::size_type>(dataBase + off), size);
      buf.setByteOrder(Endianness::big);
      comps[region].emplace_back(
          parseComponent(buf, geom[region].nOrientations));
      off += size;
    }
    regionBase += totals[region];
  }

  std::array<std::optional<Plane>, NumComponents + 1> decoded;
  std::array<std::exception_ptr, NumComponents + 1> errors;
#ifdef HAVE_OPENMP
#pragma omp taskgroup
#endif
  for (int c = 0; c != NumComponents + 1; ++c) {
#ifdef HAVE_OPENMP
#pragma omp task default(none) firstprivate(c)                                 \
    shared(comps, geom, decoded, errors)
#endif
    {
      try {
        decoded[c].emplace(decodePlane(comps, geom, c));
      } catch (...) {
        errors[c] = std::current_exception();
      }
    }
  }
  for (const std::exception_ptr& err : errors) {
    if (err)
      std::rethrow_exception(err);
  }

  const Array2DRef<const int16_t> greenHi = decoded[NumComponents]->view();
  const Array2DRef<const int16_t> greenLo = decoded[0]->view();
  const Array2DRef<const int16_t> chromaR = decoded[1]->view();
  const Array2DRef<const int16_t> chromaB = decoded[2]->view();
  if (greenLo.width() != greenHi.width())
    ThrowRDE("Green sub-band widths do not agree");
  invariant(chromaR.width() == greenLo.width() &&
            chromaR.height() == greenLo.height());
  invariant(chromaB.width() == greenLo.width() &&
            chromaB.height() == greenLo.height());
  const int h = std::min(greenLo.height(), greenHi.height());
  const int w = greenLo.width();
  invariant(2 * h <= t.dim.y && 2 * w <= t.dim.x);

  // The greens interleave back through the quincunx wavelet: first undo its
  // update step...
  const Plane low = diamondLow(greenLo, greenHi, h);

  const Array1DRef<const uint16_t> lut = delinearizationCurve();

  // ... then, row by row, undo its predict step and assemble the Bayer
  // cells: the greens land on their diagonal offset by MidValue, and each
  // 2x2 cell's R and B restore from the mean of its two greens plus the
  // doubled chroma residual. Everything clamps to the codec's bit depth and
  // maps through the delinearization curve.
  const CroppedArray2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef(),
                                        t.pos.x, t.pos.y, t.dim.x, t.dim.y);
#ifdef HAVE_OPENMP
#pragma omp taskloop default(none) shared(low)                                 \
    firstprivate(greenHi, chromaR, chromaB, out, lut, w, h)                    \
    num_tasks(rawspeed_get_number_of_processor_cores())
#endif
  for (int r = 0; r < h; ++r) {
    Array1DRef<const int16_t> lowR = low.view()[r];
    Array1DRef<const int16_t> lowN = low.view()[clampIndex(r + 1, h)];
    Array1DRef<const int16_t> hiR = greenHi[r];
    Array1DRef<const int16_t> cR = chromaR[r];
    Array1DRef<const int16_t> cB = chromaB[r];
    const auto top = out[2 * r];
    const auto bottom = out[2 * r + 1];
    for (int i = 0; i != w; ++i) {
      const int i0 = clampIndex(i - 1, w);
      const uint16_t g0 = clampBits(
          ((lowN(i0) + lowN(i) + lowR(i0) + lowR(i)) >> 2) + hiR(i) + MidValue,
          BitDepth);
      const uint16_t g1 = clampBits(lowR(i) + MidValue, BitDepth);
      const int mean = (g0 + g1) >> 1;
      const uint16_t red = clampBits(mean + 2 * cR(i), BitDepth);
      const uint16_t blue = clampBits(mean + 2 * cB(i), BitDepth);
      top(2 * i) = applyCurve ? lut(red) : red;
      top(2 * i + 1) = applyCurve ? lut(g1) : g1;
      bottom(2 * i) = applyCurve ? lut(g0) : g0;
      bottom(2 * i + 1) = applyCurve ? lut(blue) : blue;
    }
  }
}

void SonyArw6Decompressor::decompressThread() const noexcept {
#ifdef HAVE_OPENMP
#pragma omp for schedule(static)
#endif
  for (int tile = 0; tile < implicit_cast<int>(tiles.size()); ++tile) {
    try {
      decompressTile(tiles[tile]);
    } catch (const RawspeedException& err) {
      // Propagate the exception out of OpenMP magic.
      mRaw->setError(err.what());
#ifdef HAVE_OPENMP
#pragma omp cancel for
#endif
    } catch (...) {
      // We should not get any other exception type here.
      __builtin_unreachable();
    }
  }
}

void SonyArw6Decompressor::decompress() const {
#ifdef HAVE_OPENMP
#pragma omp parallel default(none)                                             \
    num_threads(rawspeed_get_number_of_processor_cores())
#endif
  decompressThread();

  std::string firstErr;
  if (mRaw->isTooManyErrors(1, &firstErr)) {
    ThrowRDE("Too many errors encountered. Giving up. First Error:\n%s",
             firstErr.c_str());
  }
}

// The decoder works in a compressed domain; each compressed code maps back
// to a linear sensor value through an inverse curve (one value per 12-bit
// code). The curve is a fixed codec constant, extracted by running samples
// through an official decoder. It is roughly logarithmic with a linear toe,
// but the exact construction is unknown, so the values are looked up in the
// table itself.
Array1DRef<const uint16_t> SonyArw6Decompressor::delinearizationCurve() {
  static constexpr std::array<uint16_t, MaxValue + 1> curve = {
      0,     1,     2,     3,     4,     5,     6,     7,     8,     9,
      10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,    62,    63,    64,    65,    66,    67,    68,    69,
      70,    71,    72,    73,    74,    75,    76,    77,    78,    79,
      80,    81,    82,    83,    84,    85,    86,    87,    88,    89,
      90,    91,    92,    93,    94,    95,    96,    97,    98,    99,
      100,   101,   102,   103,   104,   105,   106,   107,   108,   109,
      110,   111,   112,   113,   114,   115,   116,   117,   118,   119,
      120,   121,   122,   123,   124,   125,   126,   127,   128,   129,
      130,   131,   132,   133,   134,   135,   136,   137,   138,   139,
      140,   141,   142,   143,   144,   145,   146,   147,   148,   149,
      150,   151,   152,   153,   154,   155,   156,   157,   158,   159,
      160,   161,   162,   163,   164,   165,   166,   167,   168,   169,
      170,   171,   172,   173,   174,   175,   176,   177,   178,   179,
      180,   181,   182,   183,   184,   185,   186,   187,   188,   189,
      190,   191,   192,   193,   194,   195,   196,   197,   198,   199,
      200,   201,   202,   203,   204,   205,   206,   207,   208,   209,
      210,   211,   212,   213,   214,   215,   216,   217,   218,   219,
      220,   221,   222,   223,   224,   225,   226,   227,   228,   229,
      230,   231,   232,   233,   234,   235,   236,   237,   238,   239,
      240,   241,   242,   243,   244,   245,   246,   247,   248,   249,
      250,   251,   252,   253,   254,   255,   256,   257,   258,   259,
      260,   261,   262,   263,   264,   265,   266,   267,   268,   269,
      270,   271,   272,   273,   274,   275,   276,   277,   278,   279,
      280,   281,   282,   283,   284,   285,   286,   287,   288,   289,
      290,   291,   292,   293,   294,   295,   296,   297,   298,   299,
      300,   301,   302,   303,   304,   305,   306,   307,   308,   309,
      310,   311,   312,   313,   314,   315,   316,   317,   318,   319,
      320,   321,   322,   323,   324,   325,   326,   327,   328,   329,
      330,   331,   332,   333,   334,   335,   336,   337,   338,   339,
      340,   341,   342,   343,   344,   345,   346,   347,   348,   349,
      350,   351,   352,   353,   354,   355,   356,   357,   358,   359,
      360,   361,   362,   363,   364,   365,   366,   367,   368,   369,
      370,   371,   372,   373,   374,   375,   376,   377,   378,   379,
      380,   381,   382,   383,   384,   385,   386,   387,   388,   389,
      390,   391,   392,   393,   394,   395,   396,   397,   398,   399,
      400,   401,   402,   403,   404,   405,   406,   407,   408,   409,
      410,   411,   412,   413,   414,   415,   416,   417,   418,   419,
      420,   421,   422,   423,   424,   425,   426,   427,   428,   429,
      430,   431,   432,   433,   434,   435,   436,   437,   438,   439,
      440,   441,   442,   443,   444,   445,   446,   447,   448,   449,
      450,   451,   452,   453,   454,   455,   456,   457,   458,   459,
      460,   461,   462,   463,   464,   465,   466,   467,   468,   469,
      470,   471,   472,   473,   474,   475,   476,   477,   478,   479,
      480,   481,   482,   483,   484,   485,   486,   487,   488,   489,
      490,   491,   492,   493,   494,   495,   496,   497,   498,   499,
      500,   501,   502,   503,   504,   505,   506,   507,   508,   509,
      510,   511,   512,   513,   514,   515,   516,   517,   518,   519,
      520,   521,   522,   523,   524,   525,   526,   527,   528,   529,
      530,   531,   532,   533,   534,   535,   536,   537,   538,   539,
      540,   541,   542,   543,   544,   545,   546,   547,   548,   549,
      550,   551,   552,   553,   554,   555,   556,   557,   558,   559,
      560,   561,   562,   563,   564,   565,   566,   567,   568,   569,
      570,   571,   572,   573,   574,   575,   576,   577,   578,   579,
      580,   581,   582,   583,   584,   585,   586,   587,   588,   589,
      590,   591,   592,   593,   594,   595,   596,   597,   598,   599,
      600,   601,   602,   603,   604,   605,   606,   607,   608,   609,
      610,   611,   612,   613,   614,   615,   616,   617,   618,   619,
      620,   621,   622,   623,   624,   625,   626,   627,   628,   629,
      630,   631,   632,   633,   634,   635,   636,   637,   638,   639,
      640,   641,   642,   643,   644,   645,   646,   647,   648,   649,
      650,   651,   652,   653,   654,   655,   656,   657,   658,   659,
      660,   661,   662,   663,   664,   665,   666,   667,   668,   669,
      670,   671,   672,   673,   674,   675,   676,   677,   678,   679,
      680,   681,   682,   683,   684,   685,   686,   687,   688,   689,
      690,   691,   692,   693,   694,   695,   696,   697,   698,   699,
      700,   701,   702,   703,   704,   705,   706,   707,   708,   709,
      710,   711,   712,   713,   714,   715,   716,   717,   718,   719,
      720,   721,   722,   723,   724,   725,   726,   727,   728,   729,
      730,   731,   732,   733,   734,   735,   736,   737,   738,   739,
      740,   741,   742,   743,   744,   745,   746,   747,   748,   749,
      750,   751,   752,   753,   754,   755,   756,   757,   758,   759,
      760,   761,   762,   763,   764,   765,   766,   767,   768,   769,
      770,   771,   772,   773,   774,   775,   776,   777,   778,   779,
      780,   781,   782,   783,   784,   785,   786,   787,   788,   789,
      790,   791,   792,   793,   794,   795,   796,   797,   798,   799,
      800,   801,   802,   803,   804,   805,   806,   807,   808,   809,
      810,   811,   812,   813,   814,   815,   816,   817,   818,   819,
      820,   821,   822,   823,   824,   825,   826,   827,   828,   829,
      830,   831,   832,   833,   834,   835,   836,   837,   838,   839,
      840,   841,   842,   843,   844,   845,   846,   847,   848,   849,
      850,   851,   852,   853,   854,   855,   856,   857,   858,   859,
      860,   861,   862,   863,   864,   865,   866,   867,   868,   869,
      870,   871,   872,   873,   874,   875,   876,   877,   878,   879,
      880,   881,   882,   883,   884,   885,   886,   887,   888,   889,
      890,   891,   892,   893,   894,   895,   896,   897,   898,   899,
      900,   901,   902,   903,   904,   905,   906,   907,   908,   909,
      910,   911,   912,   913,   914,   915,   916,   917,   918,   919,
      920,   921,   922,   923,   924,   925,   926,   927,   928,   929,
      930,   931,   932,   933,   934,   935,   936,   937,   938,   939,
      940,   941,   942,   943,   944,   945,   946,   947,   948,   949,
      950,   951,   952,   953,   954,   955,   956,   957,   958,   959,
      960,   961,   962,   963,   964,   965,   966,   967,   968,   969,
      970,   971,   972,   973,   974,   975,   976,   977,   978,   979,
      980,   981,   982,   983,   984,   985,   986,   987,   988,   989,
      990,   991,   992,   993,   994,   995,   996,   997,   998,   999,
      1000,  1001,  1002,  1003,  1004,  1005,  1006,  1007,  1008,  1009,
      1010,  1011,  1012,  1013,  1014,  1015,  1016,  1017,  1018,  1019,
      1020,  1021,  1022,  1023,  1024,  1025,  1026,  1027,  1028,  1029,
      1030,  1031,  1032,  1033,  1034,  1035,  1036,  1037,  1038,  1039,
      1040,  1041,  1042,  1043,  1044,  1045,  1046,  1047,  1048,  1049,
      1050,  1051,  1052,  1053,  1054,  1055,  1056,  1057,  1058,  1059,
      1060,  1061,  1062,  1063,  1064,  1065,  1066,  1067,  1068,  1069,
      1070,  1071,  1072,  1073,  1074,  1075,  1076,  1077,  1078,  1079,
      1080,  1081,  1082,  1083,  1084,  1085,  1086,  1087,  1088,  1089,
      1090,  1091,  1092,  1093,  1094,  1095,  1096,  1097,  1098,  1099,
      1100,  1101,  1102,  1103,  1104,  1105,  1106,  1107,  1108,  1109,
      1110,  1111,  1112,  1113,  1114,  1115,  1116,  1117,  1118,  1119,
      1120,  1121,  1122,  1123,  1124,  1125,  1126,  1127,  1128,  1129,
      1130,  1131,  1132,  1133,  1134,  1135,  1136,  1137,  1138,  1139,
      1140,  1141,  1142,  1143,  1144,  1145,  1146,  1147,  1148,  1149,
      1150,  1151,  1152,  1153,  1154,  1155,  1156,  1157,  1158,  1159,
      1160,  1161,  1162,  1163,  1164,  1165,  1166,  1167,  1168,  1169,
      1170,  1171,  1172,  1173,  1174,  1175,  1176,  1177,  1178,  1179,
      1180,  1181,  1182,  1183,  1184,  1185,  1186,  1187,  1188,  1189,
      1190,  1191,  1192,  1193,  1194,  1195,  1196,  1197,  1198,  1199,
      1200,  1201,  1202,  1203,  1204,  1205,  1206,  1207,  1208,  1209,
      1210,  1211,  1212,  1213,  1214,  1215,  1216,  1217,  1218,  1219,
      1220,  1221,  1222,  1223,  1224,  1225,  1226,  1227,  1228,  1229,
      1230,  1231,  1232,  1233,  1234,  1235,  1236,  1237,  1238,  1239,
      1240,  1241,  1242,  1243,  1244,  1245,  1246,  1247,  1248,  1249,
      1250,  1251,  1252,  1253,  1254,  1255,  1256,  1257,  1258,  1259,
      1260,  1261,  1262,  1263,  1264,  1265,  1266,  1267,  1268,  1269,
      1270,  1271,  1272,  1273,  1274,  1275,  1276,  1277,  1278,  1279,
      1280,  1281,  1282,  1283,  1284,  1285,  1286,  1287,  1288,  1289,
      1290,  1291,  1292,  1293,  1294,  1295,  1296,  1297,  1298,  1299,
      1300,  1301,  1302,  1303,  1304,  1305,  1306,  1307,  1308,  1309,
      1310,  1311,  1312,  1313,  1314,  1315,  1316,  1317,  1318,  1319,
      1320,  1321,  1322,  1323,  1324,  1325,  1326,  1327,  1328,  1329,
      1330,  1331,  1332,  1333,  1334,  1335,  1336,  1337,  1338,  1339,
      1340,  1341,  1342,  1343,  1344,  1345,  1346,  1347,  1348,  1349,
      1350,  1351,  1352,  1353,  1354,  1355,  1356,  1357,  1358,  1359,
      1360,  1361,  1362,  1363,  1364,  1365,  1366,  1367,  1368,  1369,
      1370,  1371,  1372,  1373,  1374,  1375,  1376,  1377,  1378,  1379,
      1380,  1381,  1382,  1383,  1384,  1385,  1386,  1387,  1388,  1389,
      1390,  1391,  1392,  1393,  1394,  1395,  1396,  1397,  1398,  1399,
      1400,  1401,  1402,  1403,  1404,  1405,  1406,  1407,  1408,  1409,
      1410,  1411,  1412,  1413,  1414,  1415,  1416,  1417,  1418,  1419,
      1420,  1421,  1422,  1423,  1424,  1425,  1426,  1427,  1429,  1430,
      1431,  1432,  1433,  1434,  1435,  1436,  1438,  1439,  1440,  1441,
      1442,  1443,  1444,  1445,  1446,  1447,  1448,  1449,  1450,  1451,
      1452,  1453,  1455,  1456,  1457,  1458,  1459,  1460,  1461,  1462,
      1463,  1464,  1465,  1466,  1467,  1468,  1469,  1470,  1471,  1472,
      1473,  1474,  1475,  1476,  1477,  1478,  1480,  1481,  1482,  1483,
      1484,  1485,  1486,  1487,  1489,  1490,  1491,  1492,  1493,  1494,
      1495,  1496,  1497,  1498,  1499,  1500,  1501,  1502,  1503,  1504,
      1505,  1506,  1507,  1508,  1509,  1510,  1511,  1512,  1513,  1514,
      1515,  1516,  1517,  1518,  1519,  1520,  1522,  1523,  1524,  1525,
      1526,  1527,  1529,  1530,  1531,  1532,  1534,  1535,  1536,  1537,
      1538,  1539,  1541,  1542,  1543,  1544,  1545,  1546,  1548,  1549,
      1550,  1551,  1553,  1554,  1555,  1556,  1558,  1559,  1561,  1562,
      1563,  1565,  1566,  1567,  1568,  1569,  1571,  1572,  1573,  1574,
      1575,  1576,  1578,  1579,  1580,  1581,  1583,  1584,  1585,  1586,
      1588,  1589,  1590,  1591,  1593,  1594,  1595,  1596,  1598,  1599,
      1601,  1602,  1603,  1605,  1606,  1607,  1608,  1609,  1611,  1612,
      1613,  1614,  1615,  1616,  1618,  1619,  1621,  1622,  1623,  1625,
      1626,  1627,  1629,  1630,  1631,  1632,  1634,  1635,  1636,  1637,
      1639,  1640,  1641,  1642,  1644,  1645,  1646,  1647,  1649,  1650,
      1651,  1652,  1654,  1655,  1656,  1657,  1659,  1660,  1662,  1663,
      1664,  1666,  1667,  1668,  1670,  1671,  1673,  1674,  1675,  1677,
      1678,  1680,  1681,  1683,  1685,  1686,  1688,  1689,  1691,  1692,
      1694,  1695,  1697,  1698,  1699,  1701,  1702,  1704,  1705,  1707,
      1709,  1710,  1712,  1713,  1715,  1716,  1718,  1719,  1720,  1721,
      1723,  1724,  1725,  1727,  1728,  1730,  1731,  1733,  1734,  1736,
      1737,  1739,  1740,  1742,  1743,  1745,  1746,  1748,  1749,  1750,
      1752,  1753,  1755,  1756,  1757,  1759,  1760,  1762,  1763,  1765,
      1767,  1768,  1770,  1771,  1773,  1774,  1776,  1777,  1779,  1780,
      1781,  1783,  1784,  1786,  1787,  1789,  1791,  1792,  1794,  1795,
      1797,  1799,  1801,  1802,  1804,  1806,  1808,  1809,  1811,  1813,
      1814,  1816,  1818,  1819,  1821,  1822,  1824,  1826,  1827,  1829,
      1831,  1832,  1834,  1835,  1837,  1839,  1841,  1842,  1844,  1846,
      1848,  1849,  1851,  1853,  1854,  1856,  1858,  1859,  1861,  1862,
      1864,  1866,  1868,  1869,  1871,  1873,  1875,  1876,  1878,  1880,
      1881,  1883,  1885,  1886,  1888,  1889,  1891,  1893,  1894,  1896,
      1898,  1899,  1901,  1902,  1904,  1906,  1908,  1909,  1911,  1913,
      1915,  1916,  1918,  1920,  1922,  1924,  1926,  1927,  1929,  1931,
      1933,  1935,  1937,  1938,  1940,  1942,  1944,  1945,  1947,  1949,
      1951,  1953,  1955,  1957,  1959,  1961,  1963,  1965,  1967,  1968,
      1970,  1972,  1974,  1975,  1977,  1979,  1981,  1983,  1985,  1986,
      1988,  1990,  1992,  1994,  1996,  1998,  2000,  2001,  2003,  2005,
      2007,  2009,  2011,  2013,  2015,  2016,  2018,  2020,  2022,  2024,
      2026,  2027,  2029,  2031,  2033,  2034,  2036,  2038,  2040,  2042,
      2044,  2046,  2048,  2050,  2052,  2054,  2056,  2058,  2060,  2062,
      2064,  2066,  2068,  2070,  2072,  2074,  2076,  2078,  2080,  2082,
      2084,  2086,  2088,  2090,  2092,  2094,  2096,  2098,  2100,  2102,
      2104,  2106,  2109,  2111,  2113,  2115,  2117,  2119,  2121,  2123,
      2125,  2127,  2129,  2131,  2133,  2135,  2137,  2139,  2141,  2143,
      2145,  2147,  2149,  2151,  2153,  2155,  2157,  2159,  2161,  2163,
      2165,  2167,  2170,  2172,  2175,  2177,  2179,  2182,  2184,  2186,
      2188,  2190,  2193,  2195,  2197,  2199,  2201,  2203,  2206,  2208,
      2210,  2212,  2215,  2217,  2219,  2221,  2224,  2226,  2228,  2230,
      2233,  2235,  2237,  2239,  2242,  2244,  2247,  2249,  2251,  2254,
      2256,  2258,  2261,  2263,  2265,  2267,  2270,  2272,  2274,  2276,
      2279,  2281,  2283,  2285,  2288,  2290,  2292,  2294,  2297,  2299,
      2302,  2304,  2306,  2309,  2311,  2314,  2316,  2319,  2321,  2324,
      2326,  2329,  2331,  2333,  2336,  2338,  2340,  2342,  2345,  2347,
      2349,  2352,  2354,  2357,  2359,  2362,  2364,  2367,  2369,  2371,
      2374,  2376,  2379,  2381,  2383,  2386,  2388,  2390,  2393,  2395,
      2398,  2400,  2402,  2405,  2407,  2410,  2412,  2415,  2417,  2420,
      2422,  2425,  2427,  2430,  2432,  2435,  2438,  2440,  2443,  2445,
      2448,  2451,  2453,  2456,  2459,  2461,  2464,  2466,  2469,  2472,
      2475,  2477,  2480,  2483,  2486,  2488,  2491,  2494,  2496,  2499,
      2501,  2504,  2506,  2509,  2511,  2514,  2517,  2519,  2522,  2525,
      2528,  2530,  2533,  2536,  2538,  2541,  2543,  2546,  2548,  2551,
      2553,  2556,  2559,  2561,  2564,  2567,  2570,  2572,  2575,  2578,
      2581,  2584,  2587,  2589,  2592,  2595,  2598,  2601,  2604,  2606,
      2609,  2612,  2615,  2617,  2620,  2623,  2626,  2629,  2632,  2634,
      2637,  2640,  2643,  2646,  2649,  2652,  2655,  2657,  2660,  2663,
      2666,  2669,  2672,  2675,  2678,  2680,  2683,  2686,  2689,  2692,
      2695,  2698,  2701,  2704,  2707,  2710,  2713,  2716,  2719,  2722,
      2725,  2727,  2730,  2733,  2736,  2739,  2742,  2745,  2749,  2752,
      2755,  2758,  2761,  2764,  2767,  2770,  2773,  2776,  2779,  2782,
      2785,  2788,  2791,  2794,  2798,  2801,  2804,  2807,  2810,  2813,
      2816,  2819,  2823,  2826,  2829,  2832,  2835,  2838,  2841,  2844,
      2848,  2851,  2854,  2857,  2860,  2863,  2867,  2870,  2873,  2876,
      2880,  2883,  2886,  2889,  2893,  2896,  2899,  2902,  2906,  2909,
      2912,  2915,  2919,  2922,  2925,  2928,  2932,  2935,  2938,  2941,
      2945,  2948,  2952,  2955,  2958,  2962,  2965,  2968,  2972,  2975,
      2979,  2982,  2985,  2989,  2992,  2995,  2999,  3002,  3006,  3009,
      3012,  3016,  3019,  3023,  3026,  3030,  3033,  3037,  3040,  3044,
      3047,  3051,  3054,  3058,  3061,  3065,  3068,  3072,  3075,  3079,
      3082,  3086,  3089,  3093,  3096,  3100,  3103,  3107,  3110,  3114,
      3118,  3121,  3125,  3128,  3132,  3136,  3139,  3143,  3146,  3150,
      3153,  3157,  3160,  3164,  3168,  3171,  3175,  3179,  3183,  3186,
      3190,  3194,  3197,  3201,  3205,  3208,  3212,  3215,  3219,  3223,
      3227,  3231,  3235,  3238,  3242,  3246,  3250,  3254,  3258,  3261,
      3265,  3269,  3273,  3276,  3280,  3284,  3288,  3292,  3296,  3299,
      3303,  3307,  3311,  3315,  3319,  3323,  3327,  3331,  3335,  3339,
      3343,  3347,  3351,  3354,  3358,  3362,  3366,  3369,  3373,  3377,
      3381,  3385,  3389,  3393,  3397,  3401,  3405,  3409,  3413,  3417,
      3422,  3426,  3430,  3434,  3438,  3442,  3446,  3450,  3455,  3459,
      3463,  3467,  3471,  3475,  3479,  3483,  3487,  3491,  3495,  3499,
      3503,  3507,  3512,  3516,  3520,  3524,  3529,  3533,  3537,  3541,
      3546,  3550,  3554,  3558,  3563,  3567,  3571,  3575,  3580,  3584,
      3589,  3593,  3597,  3602,  3606,  3610,  3615,  3619,  3624,  3628,
      3632,  3637,  3641,  3645,  3650,  3654,  3658,  3662,  3667,  3671,
      3675,  3680,  3684,  3689,  3693,  3698,  3702,  3707,  3711,  3716,
      3720,  3725,  3729,  3734,  3738,  3743,  3747,  3752,  3756,  3761,
      3766,  3770,  3775,  3779,  3784,  3789,  3793,  3798,  3802,  3807,
      3811,  3816,  3820,  3825,  3830,  3834,  3839,  3844,  3849,  3853,
      3858,  3863,  3868,  3872,  3877,  3882,  3887,  3891,  3896,  3901,
      3906,  3910,  3915,  3920,  3925,  3929,  3934,  3939,  3944,  3949,
      3954,  3958,  3963,  3968,  3973,  3978,  3983,  3988,  3993,  3997,
      4002,  4007,  4012,  4017,  4022,  4027,  4032,  4037,  4042,  4047,
      4052,  4057,  4062,  4067,  4072,  4077,  4082,  4087,  4092,  4097,
      4102,  4107,  4113,  4118,  4123,  4128,  4133,  4138,  4143,  4148,
      4154,  4159,  4164,  4169,  4174,  4179,  4185,  4190,  4195,  4200,
      4206,  4211,  4216,  4221,  4227,  4232,  4237,  4242,  4248,  4253,
      4258,  4263,  4269,  4274,  4280,  4285,  4290,  4296,  4301,  4306,
      4312,  4317,  4323,  4328,  4333,  4339,  4344,  4350,  4355,  4361,
      4366,  4372,  4377,  4383,  4388,  4394,  4399,  4405,  4410,  4416,
      4421,  4427,  4432,  4438,  4444,  4449,  4455,  4461,  4467,  4472,
      4478,  4484,  4489,  4495,  4500,  4506,  4511,  4517,  4522,  4528,
      4534,  4540,  4546,  4551,  4557,  4563,  4569,  4575,  4581,  4586,
      4592,  4598,  4604,  4609,  4615,  4621,  4627,  4633,  4639,  4644,
      4650,  4656,  4662,  4668,  4674,  4680,  4686,  4692,  4698,  4704,
      4710,  4716,  4722,  4728,  4734,  4740,  4746,  4752,  4758,  4764,
      4771,  4777,  4783,  4789,  4796,  4802,  4808,  4814,  4820,  4826,
      4832,  4838,  4844,  4850,  4856,  4862,  4869,  4875,  4882,  4888,
      4894,  4901,  4907,  4913,  4920,  4926,  4933,  4939,  4945,  4952,
      4958,  4964,  4971,  4977,  4983,  4989,  4996,  5002,  5008,  5015,
      5021,  5028,  5034,  5041,  5047,  5054,  5060,  5067,  5073,  5080,
      5087,  5093,  5100,  5106,  5113,  5120,  5126,  5133,  5140,  5146,
      5153,  5159,  5166,  5173,  5180,  5186,  5193,  5200,  5207,  5213,
      5220,  5227,  5234,  5240,  5247,  5254,  5261,  5267,  5274,  5281,
      5288,  5295,  5302,  5309,  5316,  5323,  5330,  5337,  5344,  5351,
      5358,  5365,  5372,  5379,  5386,  5393,  5400,  5407,  5414,  5421,
      5428,  5435,  5442,  5449,  5456,  5463,  5471,  5478,  5485,  5492,
      5499,  5506,  5514,  5521,  5528,  5535,  5543,  5550,  5557,  5564,
      5572,  5579,  5587,  5594,  5601,  5609,  5616,  5623,  5631,  5638,
      5646,  5653,  5660,  5668,  5675,  5683,  5690,  5698,  5705,  5713,
      5720,  5728,  5735,  5743,  5751,  5758,  5766,  5774,  5782,  5789,
      5797,  5805,  5812,  5820,  5828,  5835,  5843,  5850,  5858,  5866,
      5874,  5881,  5889,  5897,  5905,  5912,  5920,  5928,  5936,  5944,
      5952,  5959,  5967,  5975,  5983,  5991,  5999,  6007,  6015,  6023,
      6031,  6039,  6047,  6055,  6063,  6071,  6079,  6087,  6095,  6103,
      6111,  6119,  6128,  6136,  6144,  6152,  6161,  6169,  6177,  6185,
      6194,  6202,  6210,  6218,  6227,  6235,  6243,  6251,  6260,  6268,
      6277,  6285,  6293,  6302,  6310,  6319,  6327,  6336,  6344,  6353,
      6361,  6370,  6378,  6387,  6395,  6404,  6412,  6421,  6429,  6438,
      6446,  6455,  6464,  6472,  6481,  6490,  6499,  6507,  6516,  6525,
      6534,  6543,  6552,  6560,  6569,  6578,  6587,  6596,  6605,  6613,
      6622,  6631,  6640,  6648,  6657,  6666,  6675,  6684,  6694,  6703,
      6712,  6721,  6730,  6739,  6748,  6757,  6767,  6776,  6785,  6794,
      6803,  6812,  6822,  6831,  6840,  6849,  6859,  6868,  6877,  6886,
      6896,  6905,  6914,  6923,  6933,  6942,  6951,  6961,  6970,  6980,
      6989,  6999,  7008,  7018,  7027,  7037,  7046,  7056,  7065,  7075,
      7084,  7094,  7103,  7113,  7123,  7132,  7142,  7152,  7162,  7171,
      7181,  7191,  7201,  7210,  7220,  7230,  7240,  7249,  7259,  7269,
      7279,  7289,  7299,  7309,  7319,  7329,  7339,  7349,  7359,  7369,
      7379,  7389,  7399,  7409,  7419,  7429,  7440,  7450,  7460,  7470,
      7481,  7491,  7501,  7511,  7522,  7532,  7543,  7553,  7563,  7574,
      7584,  7594,  7605,  7615,  7626,  7636,  7646,  7657,  7667,  7678,
      7688,  7699,  7709,  7720,  7730,  7741,  7751,  7762,  7772,  7783,
      7794,  7804,  7815,  7825,  7836,  7847,  7858,  7869,  7880,  7890,
      7901,  7912,  7923,  7934,  7945,  7956,  7967,  7977,  7988,  7999,
      8010,  8021,  8032,  8043,  8055,  8066,  8077,  8088,  8099,  8110,
      8122,  8133,  8144,  8155,  8167,  8178,  8189,  8200,  8212,  8223,
      8234,  8245,  8257,  8268,  8279,  8291,  8302,  8314,  8326,  8337,
      8349,  8360,  8372,  8384,  8395,  8407,  8418,  8430,  8441,  8453,
      8464,  8476,  8488,  8500,  8512,  8523,  8535,  8547,  8559,  8571,
      8583,  8595,  8607,  8618,  8630,  8642,  8654,  8666,  8678,  8690,
      8702,  8714,  8726,  8738,  8750,  8762,  8775,  8787,  8799,  8811,
      8824,  8836,  8848,  8860,  8873,  8885,  8898,  8910,  8922,  8935,
      8947,  8960,  8972,  8985,  8997,  9010,  9022,  9035,  9047,  9060,
      9072,  9085,  9098,  9110,  9123,  9135,  9148,  9161,  9174,  9187,
      9200,  9212,  9225,  9238,  9251,  9264,  9277,  9290,  9303,  9316,
      9329,  9342,  9355,  9368,  9381,  9394,  9408,  9421,  9434,  9447,
      9460,  9473,  9487,  9500,  9513,  9526,  9540,  9553,  9566,  9580,
      9593,  9607,  9620,  9634,  9647,  9661,  9674,  9688,  9701,  9715,
      9728,  9742,  9755,  9769,  9782,  9796,  9810,  9824,  9838,  9851,
      9865,  9879,  9893,  9907,  9921,  9935,  9949,  9963,  9977,  9991,
      10005, 10019, 10033, 10047, 10061, 10075, 10089, 10103, 10117, 10131,
      10146, 10160, 10175, 10189, 10203, 10218, 10232, 10247, 10261, 10276,
      10290, 10305, 10319, 10334, 10348, 10363, 10377, 10392, 10407, 10421,
      10436, 10450, 10465, 10480, 10495, 10509, 10524, 10539, 10554, 10568,
      10583, 10598, 10613, 10628, 10643, 10658, 10673, 10688, 10703, 10718,
      10734, 10749, 10764, 10779, 10795, 10810, 10825, 10840, 10856, 10871,
      10887, 10902, 10917, 10933, 10948, 10964, 10979, 10995, 11010, 11026,
      11041, 11057, 11072, 11088, 11104, 11120, 11136, 11151, 11167, 11183,
      11199, 11215, 11231, 11247, 11263, 11278, 11294, 11310, 11326, 11342,
      11358, 11374, 11391, 11407, 11423, 11439, 11455, 11471, 11488, 11504,
      11521, 11537, 11553, 11570, 11586, 11603, 11619, 11636, 11652, 11669,
      11685, 11702, 11718, 11735, 11752, 11768, 11785, 11802, 11819, 11835,
      11852, 11869, 11886, 11903, 11920, 11937, 11954, 11971, 11988, 12005,
      12022, 12039, 12057, 12074, 12091, 12108, 12125, 12142, 12160, 12177,
      12195, 12212, 12229, 12247, 12264, 12282, 12299, 12317, 12335, 12352,
      12370, 12387, 12405, 12423, 12441, 12458, 12476, 12494, 12512, 12529,
      12547, 12565, 12583, 12601, 12619, 12637, 12655, 12673, 12691, 12709,
      12728, 12746, 12764, 12782, 12801, 12819, 12837, 12855, 12874, 12892,
      12911, 12929, 12947, 12966, 12984, 13003, 13022, 13040, 13059, 13078,
      13097, 13115, 13134, 13153, 13172, 13191, 13210, 13228, 13247, 13266,
      13285, 13304, 13323, 13342, 13362, 13381, 13400, 13419, 13438, 13457,
      13477, 13496, 13516, 13535, 13554, 13574, 13593, 13613, 13632, 13652,
      13672, 13691, 13711, 13730, 13750, 13770, 13790, 13810, 13830, 13849,
      13869, 13889, 13909, 13929, 13949, 13969, 13990, 14010, 14030, 14050,
      14070, 14090, 14111, 14131, 14151, 14171, 14192, 14212, 14232, 14253,
      14273, 14294, 14315, 14335, 14356, 14376, 14397, 14418, 14439, 14460,
      14481, 14501, 14522, 14543, 14564, 14585, 14606, 14627, 14648, 14669,
      14690, 14711, 14732, 14753, 14775, 14796, 14818, 14839, 14860, 14882,
      14903, 14925, 14946, 14968, 14990, 15011, 15033, 15054, 15076, 15098,
      15120, 15142, 15164, 15185, 15207, 15229, 15251, 15273, 15295, 15317,
      15340, 15362, 15384, 15406, 15428, 15450, 15473, 15495, 15518, 15540,
      15562, 15585, 15607, 15630, 15653, 15675, 15698, 15721, 15744, 15766,
      15789, 15812, 15835, 15858, 15881, 15903, 15926, 15949, 15972, 15995,
      16019, 16042, 16065, 16088, 16112, 16135, 16158, 16182, 16205, 16229,
      16253, 16276, 16300, 16323, 16347, 16371, 16395, 16418, 16442, 16466,
      16490, 16513, 16537, 16561, 16585, 16609, 16634, 16658, 16682, 16706,
      16730, 16754, 16779, 16803, 16828, 16852, 16876, 16901, 16925, 16950,
      16975, 16999, 17024, 17049, 17074, 17098, 17123, 17148, 17173, 17198,
      17223, 17248, 17273, 17298, 17323, 17348, 17374, 17399, 17424, 17449,
      17475, 17500, 17525, 17551, 17576, 17602, 17628, 17653, 17679, 17704,
      17730, 17756, 17782, 17808, 17834, 17860, 17886, 17912, 17938, 17964,
      17991, 18017, 18043, 18069, 18096, 18122, 18148, 18175, 18201, 18228,
      18254, 18281, 18307, 18334, 18360, 18387, 18414, 18441, 18468, 18494,
      18521, 18548, 18575, 18602, 18630, 18657, 18684, 18711, 18739, 18766,
      18793, 18821, 18848, 18876, 18904, 18931, 18959, 18986, 19014, 19042,
      19070, 19097, 19125, 19153, 19181, 19208, 19236, 19264, 19293, 19321,
      19349, 19377, 19406, 19434, 19462, 19491, 19519, 19548, 19577, 19605,
      19634, 19662, 19691, 19720, 19749, 19778, 19807, 19835, 19864, 19893,
      19922, 19951, 19981, 20010, 20040, 20069, 20098, 20128, 20157, 20187,
      20216, 20246, 20276, 20305, 20335, 20364, 20394, 20424, 20454, 20484,
      20514, 20544, 20574, 20604, 20634, 20664, 20695, 20725, 20756, 20786,
      20816, 20847, 20877, 20908, 20939, 20969, 21000, 21031, 21062, 21092,
      21123, 21154, 21185, 21216, 21247, 21278, 21309, 21340, 21371, 21403,
      21434, 21466, 21497, 21529, 21560, 21592, 21623, 21655, 21687, 21719,
      21751, 21782, 21814, 21846, 21878, 21910, 21943, 21975, 22007, 22039,
      22072, 22104, 22136, 22169, 22202, 22234, 22267, 22300, 22333, 22365,
      22398, 22431, 22464, 22497, 22530, 22563, 22596, 22629, 22662, 22696,
      22729, 22763, 22796, 22830, 22863, 22897, 22930, 22964, 22998, 23032,
      23066, 23099, 23133, 23167, 23201, 23235, 23270, 23304, 23338, 23372,
      23407, 23441, 23475, 23510, 23545, 23579, 23614, 23649, 23684, 23718,
      23753, 23788, 23823, 23858, 23894, 23929, 23964, 23999, 24034, 24070,
      24105, 24141, 24176, 24212, 24247, 24283, 24318, 24354, 24390, 24426,
      24462, 24498, 24534, 24570, 24606, 24643, 24679, 24716, 24752, 24789,
      24825, 24862, 24898, 24935, 24972, 25009, 25046, 25082, 25119, 25156,
      25193, 25230, 25268, 25305, 25342, 25379, 25417, 25454, 25491, 25529,
      25567, 25605, 25643, 25680, 25718, 25756, 25794, 25832, 25871, 25909,
      25947, 25985, 26024, 26062, 26100, 26139, 26178, 26216, 26255, 26294,
      26333, 26371, 26410, 26449, 26488, 26527, 26567, 26606, 26645, 26684,
      26723, 26763, 26802, 26842, 26882, 26921, 26961, 27000, 27040, 27080,
      27121, 27161, 27201, 27241, 27282, 27322, 27362, 27403, 27443, 27484,
      27525, 27565, 27606, 27646, 27687, 27728, 27769, 27810, 27852, 27893,
      27934, 27975, 28016, 28058, 28099, 28141, 28183, 28224, 28266, 28307,
      28349, 28391, 28433, 28475, 28518, 28560, 28602, 28644, 28686, 28729,
      28772, 28814, 28857, 28900, 28943, 28985, 29028, 29071, 29114, 29157,
      29201, 29244, 29287, 29330, 29373, 29417, 29461, 29504, 29548, 29592,
      29636, 29679, 29723, 29767, 29812, 29856, 29900, 29944, 29989, 30033,
      30077, 30122, 30167, 30211, 30256, 30301, 30346, 30390, 30435, 30480,
      30526, 30571, 30617, 30662, 30707, 30753, 30798, 30844, 30890, 30936,
      30982, 31027, 31073, 31119, 31165, 31212, 31258, 31305, 31351, 31398,
      31444, 31491, 31537, 31584, 31631, 31678, 31725, 31772, 31819, 31866,
      31913, 31961, 32008, 32056, 32104, 32151, 32199, 32246, 32294, 32342,
      32390, 32438, 32487, 32535, 32583, 32631, 32679, 32766, 32777, 32825,
      32874, 32923, 32972, 33020, 33069, 33118, 33168, 33217, 33267, 33316,
      33365, 33415, 33464, 33514, 33564, 33614, 33664, 33714, 33764, 33814,
      33864, 33915, 33965, 34016, 34067, 34117, 34168, 34218, 34269, 34320,
      34371, 34422, 34474, 34525, 34576, 34627, 34678, 34730, 34782, 34834,
      34886, 34937, 34989, 35041, 35093, 35146, 35198, 35251, 35303, 35356,
      35408, 35461, 35513, 35566, 35619, 35672, 35725, 35778, 35831, 35884,
      35937, 35991, 36045, 36098, 36152, 36206, 36260, 36313, 36367, 36422,
      36476, 36531, 36585, 36640, 36694, 36749, 36803, 36858, 36913, 36968,
      37023, 37078, 37133, 37188, 37243, 37299, 37355, 37410, 37466, 37522,
      37578, 37633, 37689, 37746, 37802, 37859, 37915, 37972, 38028, 38085,
      38141, 38198, 38255, 38312, 38369, 38426, 38483, 38540, 38597, 38655,
      38713, 38771, 38829, 38886, 38944, 39002};
  return {curve.data(), MaxValue + 1};
}

} // namespace rawspeed
