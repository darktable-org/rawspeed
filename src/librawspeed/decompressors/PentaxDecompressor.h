/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser

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

#include "rawspeedconfig.h"                                 // for HAVE_PTHREAD
#include "common/Common.h"                                  // for uint32
#include "common/RawImage.h"                                // for RawImage
#include "decompressors/AbstractParallelizedDecompressor.h" // for AbstractPar..
#include "decompressors/HuffmanTable.h"                     // for HuffmanTable
#include "io/ByteStream.h"                                  // for ByteStream
#include <algorithm>                                        // for move

namespace rawspeed {

class TiffIFD;

class PentaxDecompressor final : public AbstractParallelizedDecompressor {
  const ByteStream input;
  const TiffIFD* root;
  const HuffmanTable ht;

  enum class ThreadingModel {
    // no threading whatsoever, need to do everything right here and now.
    NoThreading = 0,

    // threaded. only do the preparatory work that has to be done sequentially
    MainThread = 1,

    // threaded. runs strictly after MainThread. finishes decoding parallelized
    SlaveThread = 2,
  };

  template <ThreadingModel t>
  void decompressInternal(int row_start, int row_end) const;

  static HuffmanTable SetupHuffmanTable_Legacy();
  static HuffmanTable SetupHuffmanTable_Modern(const TiffIFD* root);
  static HuffmanTable SetupHuffmanTable(const TiffIFD* root);

  static const uchar8 pentax_tree[][2][16];

#ifdef HAVE_PTHREAD
  void decompressThreaded(const RawDecompressorThread* t) const final;
#endif

  // according to benchmarks, does not appear to be profitable.
  static constexpr const bool disableThreading = true;

public:
  PentaxDecompressor(ByteStream input_, const RawImage& img, TiffIFD* root_)
      : AbstractParallelizedDecompressor(img), input(std::move(input_)),
        root(root_), ht(SetupHuffmanTable(root)) {}

  void decompress() const;
};

} // namespace rawspeed
