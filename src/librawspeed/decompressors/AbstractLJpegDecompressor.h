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

#include "common/Common.h"                      // for uint32, ushort16
#include "common/RawImage.h"                    // for RawImage
#include "decoders/RawDecoderException.h"       // for ThrowRDE
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "decompressors/HuffmanTable.h"         // for HuffmanTable
#include "io/Buffer.h"                          // for Buffer, Buffer::size_type
#include "io/ByteStream.h"                      // for ByteStream
#include "io/Endianness.h" // for getHostEndianness, Endiannes...
#include <array>           // for array
#include <memory>          // for unique_ptr
#include <vector>          // for vector

/*
 * The following enum and two structs are stolen from the IJG JPEG library
 * Comments added by tm. See also Copyright in HuffmanTable.h.
 */

namespace rawspeed {

enum JpegMarker { /* JPEG marker codes			*/
  M_STUFF = 0x00,
  M_SOF0  = 0xc0,	/* baseline DCT				*/
  M_SOF1  = 0xc1,	/* extended sequential DCT		*/
  M_SOF2  = 0xc2,	/* progressive DCT			*/
  M_SOF3  = 0xc3,	/* lossless (sequential)		*/

  M_SOF5  = 0xc5,	/* differential sequential DCT		*/
  M_SOF6  = 0xc6,	/* differential progressive DCT		*/
  M_SOF7  = 0xc7,	/* differential lossless		*/

  M_JPG   = 0xc8,	/* JPEG extensions			*/
  M_SOF9  = 0xc9,	/* extended sequential DCT		*/
  M_SOF10 = 0xca,	/* progressive DCT			*/
  M_SOF11 = 0xcb,	/* lossless (sequential)		*/

  M_SOF13 = 0xcd,	/* differential sequential DCT		*/
  M_SOF14 = 0xce,	/* differential progressive DCT		*/
  M_SOF15 = 0xcf,	/* differential lossless		*/

  M_DHT   = 0xc4,	/* define Huffman tables		*/

  M_DAC   = 0xcc,	/* define arithmetic conditioning table	*/

  M_RST0  = 0xd0,	/* restart				*/
  M_RST1  = 0xd1,	/* restart				*/
  M_RST2  = 0xd2,	/* restart				*/
  M_RST3  = 0xd3,	/* restart				*/
  M_RST4  = 0xd4,	/* restart				*/
  M_RST5  = 0xd5,	/* restart				*/
  M_RST6  = 0xd6,	/* restart				*/
  M_RST7  = 0xd7,	/* restart				*/

  M_SOI   = 0xd8,	/* start of image			*/
  M_EOI   = 0xd9,	/* end of image				*/
  M_SOS   = 0xda,	/* start of scan			*/
  M_DQT   = 0xdb,	/* define quantization tables		*/
  M_DNL   = 0xdc,	/* define number of lines		*/
  M_DRI   = 0xdd,	/* define restart interval		*/
  M_DHP   = 0xde,	/* define hierarchical progression	*/
  M_EXP   = 0xdf,	/* expand reference image(s)		*/

  M_APP0  = 0xe0,	/* application marker, used for JFIF	*/
  M_APP1  = 0xe1,	/* application marker			*/
  M_APP2  = 0xe2,	/* application marker			*/
  M_APP3  = 0xe3,	/* application marker			*/
  M_APP4  = 0xe4,	/* application marker			*/
  M_APP5  = 0xe5,	/* application marker			*/
  M_APP6  = 0xe6,	/* application marker			*/
  M_APP7  = 0xe7,	/* application marker			*/
  M_APP8  = 0xe8,	/* application marker			*/
  M_APP9  = 0xe9,	/* application marker			*/
  M_APP10 = 0xea,	/* application marker			*/
  M_APP11 = 0xeb,	/* application marker			*/
  M_APP12 = 0xec,	/* application marker			*/
  M_APP13 = 0xed,	/* application marker			*/
  M_APP14 = 0xee,	/* application marker, used by Adobe	*/
  M_APP15 = 0xef,	/* application marker			*/

  M_JPG0  = 0xf0,	/* reserved for JPEG extensions		*/
  M_JPG13 = 0xfd,	/* reserved for JPEG extensions		*/
  M_COM   = 0xfe,	/* comment				*/

  M_TEM   = 0x01,	/* temporary use			*/
  M_FILL  = 0xFF


};

/*
* The following structure stores basic information about one component.
*/
struct JpegComponentInfo {
  /*
  * These values are fixed over the whole image.
  * They are read from the SOF marker.
  */
  uint32 componentId = -1;		/* identifier for this component (0..255) */

  /*
  * Huffman table selector (0..3). The value may vary
  * between scans. It is read from the SOS marker.
  */
  uint32 dcTblNo = -1;
  uint32 superH = -1; // Horizontal Supersampling
  uint32 superV = -1; // Vertical Supersampling
};

class SOFInfo {
public:
  JpegComponentInfo compInfo[4];
  uint32 w = 0;    // Width
  uint32 h = 0;    // Height
  uint32 cps = 0;  // Components
  uint32 prec = 0; // Precision
  bool initialized = false;
};

class AbstractLJpegDecompressor : public AbstractDecompressor {
  // std::vector of unique HTs, to not recreate HT, but cache them
  std::vector<std::unique_ptr<HuffmanTable>> huffmanTableStore;
  HuffmanTable ht_;      // temporary table, used

public:
  AbstractLJpegDecompressor(const Buffer& data, Buffer::size_type offset,
                            Buffer::size_type size, const RawImage& img)
      : input(data, offset, size, Endianness::big), mRaw(img) {}
  AbstractLJpegDecompressor(const Buffer& data, Buffer::size_type offset,
                            const RawImage& img)
      : AbstractLJpegDecompressor(data, offset, data.getSize() - offset, img) {}
  virtual ~AbstractLJpegDecompressor() = default;

protected:
  bool fixDng16Bug = false;  // DNG v1.0.x compatibility
  bool fullDecodeHT = true;  // FullDecode Huffman

  void decode();
  void parseSOF(SOFInfo* i);
  void parseSOS();
  void parseDHT();
  JpegMarker getNextMarker(bool allowskip);

  template <int N_COMP>
  std::array<HuffmanTable*, N_COMP> getHuffmanTables() const {
    std::array<HuffmanTable*, N_COMP> ht;
    for (int i = 0; i < N_COMP; ++i) {
      const auto dcTblNo = frame.compInfo[i].dcTblNo;
      if (dcTblNo > huff.size()) {
        ThrowRDE("Decoding table %u for comp %i does not exist (tables = %lu)",
                 dcTblNo, i, huff.size());
      }
      ht[i] = huff[dcTblNo];
    }

    return ht;
  }

  template <int N_COMP>
  __attribute__((pure)) std::array<ushort16, N_COMP>
  getInitialPredictors() const {
    std::array<ushort16, N_COMP> pred;
    pred.fill(1 << (frame.prec - Pt - 1));
    return pred;
  }

  virtual void decodeScan() = 0;

  ByteStream input;
  RawImage mRaw;

  SOFInfo frame;
  uint32 predictorMode = 0;
  uint32 Pt = 0;
  std::array<HuffmanTable*, 4> huff{{}}; // 4 pointers into the store
};

} // namespace rawspeed
