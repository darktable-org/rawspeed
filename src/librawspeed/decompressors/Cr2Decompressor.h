/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Axel Waggershauser
    Copyright (C) 2018 Roman Lebedev

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

#pragma once

#include "common/Point.h"                    // for iPoint2D
#include "common/RawImage.h"                 // for RawImage
#include "common/RawspeedException.h"        // for ThrowException
#include "common/iterator_range.h"           // for iterator_range
#include "decoders/RawDecoderException.h"    // for ThrowException, ThrowRDE
#include "decompressors/DummyHuffmanTable.h" // for DummyHuffmanTable
#include "decompressors/HuffmanTable.h" // for HuffmanTable, HuffmanTableLUT
#include "io/ByteStream.h"              // for ByteStream
#include <array>                        // for array
#include <cassert>                      // for assert
#include <cstddef>                      // for size_t
#include <cstdint>                      // for uint16_t
#include <functional>                   // for reference_wrapper
#include <tuple>                        // for array, tuple
#include <utility>                      // for index_sequence
#include <vector>                       // for vector

namespace rawspeed {

class ByteStream;
class RawImage;

struct Cr2SliceWidthIterator;
struct Cr2SliceIterator;
struct Cr2OutputTileIterator;
class Cr2VerticalOutputStripIterator;

class Cr2SliceWidths {
  int numSlices = 0;
  int sliceWidth = 0;
  int lastSliceWidth = 0;

  friend class Cr2LJpegDecoder;
  friend struct Cr2SliceWidthIterator;

  template <typename HuffmanTable> friend class Cr2Decompressor;

public:
  Cr2SliceWidths() = default;

  Cr2SliceWidths(uint16_t numSlices_, uint16_t sliceWidth_,
                 uint16_t lastSliceWidth_)
      : numSlices(numSlices_), sliceWidth(sliceWidth_),
        lastSliceWidth(lastSliceWidth_) {
    if (numSlices < 1)
      ThrowRDE("Bad slice count: %u", numSlices);
  }

  [[nodiscard]] bool empty() const {
    return 0 == numSlices && 0 == sliceWidth && 0 == lastSliceWidth;
  }

  [[nodiscard]] int widthOfSlice(int sliceId) const {
    assert(sliceId >= 0 && sliceId < numSlices);
    if ((sliceId + 1) == numSlices)
      return lastSliceWidth;
    return sliceWidth;
  }

  [[nodiscard]] Cr2SliceWidthIterator begin() const;
  [[nodiscard]] Cr2SliceWidthIterator end() const;
};

struct Cr2SliceWidthIterator final {
  const Cr2SliceWidths& slicing;

  int sliceId;

  using iterator_category = std::bidirectional_iterator_tag;
  using difference_type = std::ptrdiff_t;
  using value_type = int;
  using pointer = const value_type*;   // Unusable, but must be here.
  using reference = const value_type&; // Unusable, but must be here.

  Cr2SliceWidthIterator(const Cr2SliceWidths& slicing_, int sliceId_)
      : slicing(slicing_), sliceId(sliceId_) {
    assert(sliceId >= 0 && sliceId <= slicing.numSlices && "Iterator overflow");
  }

  value_type operator*() const {
    assert(sliceId >= 0 && sliceId < slicing.numSlices && "Iterator overflow");
    return slicing.widthOfSlice(sliceId);
  }
  Cr2SliceWidthIterator& operator++() {
    ++sliceId;
    return *this;
  }
  Cr2SliceWidthIterator& operator--() {
    --sliceId;
    return *this;
  }
  friend bool operator==(const Cr2SliceWidthIterator& a,
                         const Cr2SliceWidthIterator& b) {
    assert(&a.slicing == &b.slicing && "Comparing unrelated iterators.");
    return a.sliceId == b.sliceId;
  }
  friend bool operator!=(const Cr2SliceWidthIterator& a,
                         const Cr2SliceWidthIterator& b) {
    return !(a == b);
  }
};

inline Cr2SliceWidthIterator Cr2SliceWidths::begin() const {
  return {*this, 0};
}
inline Cr2SliceWidthIterator Cr2SliceWidths::end() const {
  return {*this, numSlices};
}

template <typename HuffmanTable> class Cr2Decompressor final {
public:
  struct PerComponentRecipe {
    const HuffmanTable& ht;
    const uint16_t initPred;
  };

private:
  const RawImage mRaw;
  const std::tuple<int /*N_COMP*/, int /*X_S_F*/, int /*Y_S_F*/> format;
  iPoint2D dim;
  iPoint2D frame;
  Cr2SliceWidths slicing;

  const std::vector<PerComponentRecipe> rec;

  const ByteStream input;

  template <int N_COMP, size_t... I>
  [[nodiscard]] std::array<std::reference_wrapper<const HuffmanTable>, N_COMP>
      getHuffmanTablesImpl(std::index_sequence<I...> /*unused*/) const;

  template <int N_COMP>
  [[nodiscard]] std::array<std::reference_wrapper<const HuffmanTable>, N_COMP>
  getHuffmanTables() const;

  template <int N_COMP>
  [[nodiscard]] std::array<uint16_t, N_COMP> getInitialPreds() const;

  template <int N_COMP, int X_S_F, int Y_S_F> void decompressN_X_Y();

  [[nodiscard]] iterator_range<Cr2SliceIterator> getSlices();
  [[nodiscard]] iterator_range<Cr2OutputTileIterator> getAllOutputTiles();
  [[nodiscard]] iterator_range<Cr2OutputTileIterator> getOutputTiles();
  [[nodiscard]] iterator_range<Cr2VerticalOutputStripIterator>
  getVerticalOutputStrips();

public:
  Cr2Decompressor(
      const RawImage& mRaw,
      std::tuple<int /*N_COMP*/, int /*X_S_F*/, int /*Y_S_F*/> format,
      iPoint2D frame, Cr2SliceWidths slicing,
      std::vector<PerComponentRecipe> rec, ByteStream input);

  void decompress();
};

extern template class Cr2Decompressor<HuffmanTable>;

} // namespace rawspeed
