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

#include "adt/Casts.h"
#include "codes/AbstractPrefixCode.h"
#include "codes/HuffmanCode.h"
#include "codes/PrefixCodeDecoder.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/AbstractDecompressor.h"
#include "decompressors/JpegMarkers.h"
#include "io/ByteStream.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

/*
 * The two following structs are stolen from the IJG JPEG library
 * Comments added by tm. See also Copyright in PrefixCodeDecoder.h.
 */

namespace rawspeed {

/*
 * The following structure stores basic information about one component.
 */
struct JpegComponentInfo final {
  /*
   * These values are fixed over the whole image.
   * They are read from the SOF marker.
   */
  uint32_t componentId = ~0U; /* identifier for this component (0..255) */

  /*
   * Huffman table selector (0..3). The value may vary
   * between scans. It is read from the SOS marker.
   */
  uint32_t dcTblNo = ~0U;
  uint32_t superH = ~0U; // Horizontal Supersampling
  uint32_t superV = ~0U; // Vertical Supersampling
};

class SOFInfo final {
public:
  std::array<JpegComponentInfo, 4> compInfo;
  uint32_t w = 0;    // Width
  uint32_t h = 0;    // Height
  uint32_t cps = 0;  // Components
  uint32_t prec = 0; // Precision
  bool initialized = false;
};

class AbstractLJpegDecoder : public AbstractDecompressor {
  // std::vector of unique HTs, to not recreate HT, but cache them
  std::vector<std::unique_ptr<const HuffmanCode<BaselineCodeTag>>>
      huffmanCodeStore;
  std::vector<std::unique_ptr<const PrefixCodeDecoder<>>>
      PrefixCodeDecoderStore;

  uint32_t Pt = 0;
  std::array<const PrefixCodeDecoder<>*, 4> huff{
      {}}; // 4 pointers into the store

  virtual void anchor() const;

public:
  AbstractLJpegDecoder(ByteStream bs, RawImage img);
  [[nodiscard]] int getSamplePrecision() const { return frame.prec; }

  virtual ~AbstractLJpegDecoder() = default;

protected:
  bool fixDng16Bug = false; // DNG v1.0.x compatibility
  bool fullDecodeHT = true; // FullDecode Huffman

  // Certain non-standard-complaint LJpeg's (old Hasselblad cameras) might not
  // end with an EOI marker. This erratum considers an implicit EOI marker
  // to be present after the (first) full Scan.
  [[nodiscard]] virtual bool erratumImplicitEOIMarkerAfterScan() const {
    return false;
  }

  void decodeSOI();
  void parseSOF(ByteStream data, SOFInfo* i);
  void parseSOS(ByteStream data);
  void parseDHT(ByteStream data);
  void parseDRI(ByteStream dri);
  JpegMarker getNextMarker(bool allowskip);

  [[nodiscard]] std::vector<const PrefixCodeDecoder<>*>
  getPrefixCodeDecoders(int N_COMP) const {
    std::vector<const PrefixCodeDecoder<>*> ht(N_COMP);
    for (int i = 0; i < N_COMP; ++i) {
      const unsigned dcTblNo = frame.compInfo[i].dcTblNo;
      if (const auto dcTbls = implicit_cast<unsigned>(huff.size());
          dcTblNo >= dcTbls) {
        ThrowRDE("Decoding table %u for comp %i does not exist (tables = %u)",
                 dcTblNo, i, dcTbls);
      }
      ht[i] = huff[dcTblNo];
    }

    return ht;
  }

  [[nodiscard]] std::vector<uint16_t> getInitialPredictors(int N_COMP) const {
    std::vector<uint16_t> pred(N_COMP);
    if (frame.prec < (Pt + 1)) {
      ThrowRDE("Invalid precision (%u) and point transform (%u) combination!",
               frame.prec, Pt);
    }
    std::fill(pred.begin(), pred.end(), 1 << (frame.prec - Pt - 1));
    return pred;
  }

  [[nodiscard]] virtual ByteStream::size_type decodeScan() = 0;

  ByteStream input;
  RawImage mRaw;

  SOFInfo frame;
  uint16_t numMCUsPerRestartInterval = 0;
  uint32_t predictorMode = 0;
};

} // namespace rawspeed
