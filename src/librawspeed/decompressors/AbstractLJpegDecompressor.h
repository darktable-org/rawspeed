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

#include "common/RawImage.h"                    // for RawImage
#include "decoders/RawDecoderException.h"       // for ThrowRDE
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "decompressors/HuffmanTable.h"         // for HuffmanTable
#include "io/ByteStream.h"                      // for ByteStream
#include <array>                                // for array
#include <cstdint>                              // for uint32_t, uint16_t
#include <memory>                               // for unique_ptr
#include <vector>                               // for vector

/*
 * The following enum and two structs are stolen from the IJG JPEG library
 * Comments added by tm. See also Copyright in HuffmanTable.h.
 */

namespace rawspeed {

enum class JpegMarker { /* JPEG marker codes			*/
                        STUFF = 0x00,
                        SOF0 = 0xc0, /* baseline DCT */
                        SOF1 = 0xc1, /* extended sequential DCT		*/
                        SOF2 = 0xc2, /* progressive DCT			*/
                        SOF3 = 0xc3, /* lossless (sequential)		*/

                        SOF5 = 0xc5, /* differential sequential DCT
                                      */
                        SOF6 = 0xc6, /* differential progressive DCT
                                      */
                        SOF7 = 0xc7, /* differential lossless		*/

                        JPG = 0xc8,   /* JPEG extensions			*/
                        SOF9 = 0xc9,  /* extended sequential DCT		*/
                        SOF10 = 0xca, /* progressive DCT */
                        SOF11 = 0xcb, /* lossless (sequential)		*/

                        SOF13 = 0xcd, /* differential sequential DCT
                                       */
                        SOF14 = 0xce, /* differential progressive DCT
                                       */
                        SOF15 = 0xcf, /* differential lossless		*/

                        DHT = 0xc4, /* define Huffman tables		*/

                        DAC = 0xcc, /* define arithmetic conditioning table
                                     */

                        RST0 = 0xd0, /* restart				*/
                        RST1 = 0xd1, /* restart				*/
                        RST2 = 0xd2, /* restart				*/
                        RST3 = 0xd3, /* restart				*/
                        RST4 = 0xd4, /* restart				*/
                        RST5 = 0xd5, /* restart				*/
                        RST6 = 0xd6, /* restart				*/
                        RST7 = 0xd7, /* restart				*/

                        SOI = 0xd8, /* start of image			*/
                        EOI = 0xd9, /* end of image */
                        SOS = 0xda, /* start of scan			*/
                        DQT =
                            0xdb,   /* define quantization tables		*/
                        DNL = 0xdc, /* define number of lines		*/
                        DRI = 0xdd, /* define restart interval		*/
                        DHP = 0xde, /* define hierarchical progression	*/
                        EXP =
                            0xdf, /* expand reference image(s)		*/

                        APP0 =
                            0xe0,     /* application marker, used for JFIF	*/
                        APP1 = 0xe1,  /* application marker  */
                        APP2 = 0xe2,  /* application marker  */
                        APP3 = 0xe3,  /* application marker  */
                        APP4 = 0xe4,  /* application marker  */
                        APP5 = 0xe5,  /* application marker  */
                        APP6 = 0xe6,  /* application marker  */
                        APP7 = 0xe7,  /* application marker  */
                        APP8 = 0xe8,  /* application marker  */
                        APP9 = 0xe9,  /* application marker  */
                        APP10 = 0xea, /* application marker */
                        APP11 = 0xeb, /* application marker */
                        APP12 = 0xec, /* application marker */
                        APP13 = 0xed, /* application marker */
                        APP14 =
                            0xee,     /* application marker, used by Adobe	*/
                        APP15 = 0xef, /* application marker */

                        JPG0 = 0xf0,  /* reserved for JPEG extensions
                                       */
                        JPG13 = 0xfd, /* reserved for JPEG extensions
                                       */
                        COM = 0xfe,   /* comment				*/

                        TEM = 0x01, /* temporary use			*/
                        FILL = 0xFF

};

/*
* The following structure stores basic information about one component.
*/
struct JpegComponentInfo {
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

class SOFInfo {
public:
  std::array<JpegComponentInfo, 4> compInfo;
  uint32_t w = 0;    // Width
  uint32_t h = 0;    // Height
  uint32_t cps = 0;  // Components
  uint32_t prec = 0; // Precision
  bool initialized = false;
};

class AbstractLJpegDecompressor : public AbstractDecompressor {
  // std::vector of unique HTs, to not recreate HT, but cache them
  std::vector<std::unique_ptr<HuffmanTable>> huffmanTableStore;
  HuffmanTable ht_;      // temporary table, used

  uint32_t Pt = 0;
  std::array<HuffmanTable*, 4> huff{{}}; // 4 pointers into the store

public:
  AbstractLJpegDecompressor(ByteStream bs, RawImageData* img);

  virtual ~AbstractLJpegDecompressor() = default;

protected:
  bool fixDng16Bug = false;  // DNG v1.0.x compatibility
  bool fullDecodeHT = true;  // FullDecode Huffman

  void decode();
  void parseSOF(ByteStream data, SOFInfo* i);
  void parseSOS(ByteStream data);
  void parseDHT(ByteStream data);
  JpegMarker getNextMarker(bool allowskip);

  template <int N_COMP>
  [[nodiscard]] [[nodiscard]] [[nodiscard]] std::array<HuffmanTable*, N_COMP>
  getHuffmanTables() const {
    std::array<HuffmanTable*, N_COMP> ht;
    for (int i = 0; i < N_COMP; ++i) {
      const unsigned dcTblNo = frame.compInfo[i].dcTblNo;
      if (const unsigned dcTbls = huff.size(); dcTblNo >= dcTbls) {
        ThrowRDE("Decoding table %u for comp %i does not exist (tables = %u)",
                 dcTblNo, i, dcTbls);
      }
      ht[i] = huff[dcTblNo];
    }

    return ht;
  }

  template <int N_COMP>
  [[nodiscard]] [[nodiscard]] [[nodiscard]] __attribute__((pure))
  std::array<uint16_t, N_COMP>
  getInitialPredictors() const {
    std::array<uint16_t, N_COMP> pred;
    if (frame.prec < (Pt + 1)) {
      ThrowRDE("Invalid precision (%u) and point transform (%u) combination!",
               frame.prec, Pt);
    }
    pred.fill(1 << (frame.prec - Pt - 1));
    return pred;
  }

  virtual void decodeScan() = 0;

  ByteStream input;
  RawImageData* mRaw;

  SOFInfo frame;
  uint32_t predictorMode = 0;
};

} // namespace rawspeed
