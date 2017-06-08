/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "decoders/X3fDecoder.h"
#include "common/Common.h"                // for ushort16, uint32, uchar8
#include "common/Memory.h"                // for alignedFree, alignedMalloc...
#include "common/Point.h"                 // for iPoint2D, iRectangle2D
#include "common/RawspeedException.h"     // for RawspeedException
#include "decoders/RawDecoderException.h" // for RawDecoderException (ptr o...
#include "decompressors/HuffmanTable.h"   // for HuffmanTable
#include "io/Buffer.h"                    // for Buffer, Buffer::size_type
#include "io/ByteStream.h"                // for ByteStream
#include "parsers/TiffParser.h"           // for TiffParser
#include "tiff/TiffEntry.h"               // IWYU pragma: keep
#include "tiff/TiffIFD.h"                 // for TiffID, TiffRootIFD, TiffR...
#include <algorithm>                      // for max
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstring>                        // for memset
#include <istream>                        // for basic_istream::operator>>
#include <map>                            // for map, _Rb_tree_iterator
#include <memory>                         // for unique_ptr
#include <string>                         // for string
#include <utility>                        // for pair
#include <vector>                         // for vector

using std::string;
using std::unique_ptr;
using std::max;

namespace rawspeed {

X3fDecoder::X3fDecoder(const Buffer* file) : RawDecoder(file){};

X3fDecoder::~X3fDecoder() {
  if (huge_table)
    alignedFree(huge_table);
  if (line_offsets)
    alignedFree(line_offsets);
  huge_table = nullptr;
  line_offsets = nullptr;
}

string X3fDecoder::getIdAsString(ByteStream *bytes_) {
  uchar8 id[5];
  for (int i = 0; i < 4; i++)
    id[i] = bytes_->getByte();
  id[4] = 0;
  return string(reinterpret_cast<const char*>(id));
}


RawImage X3fDecoder::decodeRawInternal()
{
  auto img = mImages.begin();
  for (; img !=  mImages.end(); ++img) {
    X3fImage cimg = *img;
    if (cimg.type == 1 || cimg.type == 3) {
      decompressSigma(cimg);
      break;
    }
  }
  return mRaw;
}

void X3fDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  if (readName() && checkCameraSupported(meta, camera_make, camera_model, "")) {
    int iso = 0;
    if (hasProp("ISO")) {
      std::istringstream iss(getProp("ISO"));
      iss >> iso;
    }
    setMetaData(meta, camera_make, camera_model, "", iso);
  }
}

// readName will read the make and model of the image.
//
// If the name is read, it will return true, and the make/model
// will be available in camera_make/camera_model members.
bool X3fDecoder::readName() {
  if (camera_make.length() != 0 && camera_model.length() != 0) {
    return true;
  }

  // Read from properties
  if (hasProp("CAMMANUF") && hasProp("CAMMODEL")) {
    camera_make = getProp("CAMMANUF");
    camera_model = getProp("CAMMODEL");
    return true;
  }

  // See if we can find EXIF info and grab the name from there.
  // This is needed for Sigma DP2 Quattro and possibly later cameras.
  auto img = mImages.begin();
  for (; img !=  mImages.end(); ++img) {
    X3fImage cimg = *img;
    if (cimg.type == 2 && cimg.format == 0x12 && cimg.dataSize > 100) {
      if (!mFile->isValid(cimg.dataOffset, cimg.dataSize )) {
        return false;
      }
      ByteStream i(mFile, cimg.dataOffset, cimg.dataSize);
      // Skip jpeg header
      i.skipBytes(6);
      if (i.getU32() == 0x66697845) { // Match text 'Exif'
        try {
          TiffRootIFDOwner root = TiffParser::parse(
              mFile->getSubView(cimg.dataOffset + 12, i.getRemainSize()));
          auto id = root->getID();
          camera_model = id.model;
          camera_make = id.make;
          mProperties.props["CAMMANUF"] = id.make;
          mProperties.props["CAMMODEL"] = id.model;
          return true;
        } catch (RawspeedException&) {
          return false;
        }
      }
    }
  }
  return false;
}

void X3fDecoder::checkSupportInternal(const CameraMetaData* meta) {
  if (readName()) {
    if (!checkCameraSupported(meta, camera_make, camera_model, "" ))
      ThrowRDE("Unknown camera. Will not guess.");
    return;
  }

  // If we somehow got to here without a camera, see if we have an image
  // with proper format identifiers.
  auto img = mImages.begin();
  for (; img !=  mImages.end(); ++img) {
    X3fImage cimg = *img;
    if ((cimg.type == 1 || cimg.type == 3) &&
        (cimg.format == 30 || cimg.format == 35))
      return;
  }
  ThrowRDE("Unable to determine camera name.");
}

string X3fDecoder::getProp(const char* key )
{
  auto prop_it = mProperties.props.find(key);
  if (prop_it != mProperties.props.end())
    return (*prop_it).second;
  return nullptr;
}

void X3fDecoder::decompressSigma(const X3fImage& image) {
  ByteStream input(mFile, image.dataOffset, image.dataSize);
  mRaw->dim.x = image.width;
  mRaw->dim.y = image.height;
  mRaw->setCpp(3);
  mRaw->isCFA = false;
  mRaw->createData();
  curr_image = &image;
  int bits = 13;

  if (image.format == 35) {
    for (auto &i : planeDim) {
      i.x = input.getU16();
      i.y = input.getU16();
    }
    bits = 15;
  }
  if (image.format == 30 || image.format == 35) {
    for (int &i : pred)
      i = input.getU16();

    // Skip padding
    input.skipBytes(2);

    createSigmaTable(&input, bits);

    // Skip padding  (2 x 0x00)
    if (image.format == 35) {
      input.skipBytes(2+4);
      plane_offset[0] = image.dataOffset + 68;
    } else {
      // Skip padding  (2 x 0x00)
      input.skipBytes(2);
      plane_offset[0] = image.dataOffset + 48;
    }

    for (int i = 0; i < 3; i++) {
      plane_sizes[i] = input.getU32();
      // Planes are 16 byte aligned
      if (i != 2) {
        plane_offset[i + 1] = plane_offset[i] + roundUp(plane_sizes[i], 16);
        if (plane_offset[i]>mFile->getSize())
          ThrowRDE("Plane offset outside image");
      }
    }
    mRaw->clearArea(iRectangle2D(0,0,image.width,image.height));

    startTasks(3);
    //Interpolate based on blue value
    if (image.format == 35) {
      int w = planeDim[0].x;
      int h = planeDim[0].y;
      for (int i = 0; i < 2;  i++) {
        for (int y = 0; y < h; y++) {
          ushort16* dst =
              reinterpret_cast<ushort16*>(mRaw->getData(0, y * 2)) + i;
          ushort16* dst_down =
              reinterpret_cast<ushort16*>(mRaw->getData(0, y * 2 + 1)) + i;
          ushort16* blue =
              reinterpret_cast<ushort16*>(mRaw->getData(0, y * 2)) + 2;
          ushort16* blue_down =
              reinterpret_cast<ushort16*>(mRaw->getData(0, y * 2 + 1)) + 2;
          for (int x = 0; x < w; x++) {
            // Interpolate 1 missing pixel
            int blue_mid =
                (static_cast<int>(blue[0]) + static_cast<int>(blue[3]) +
                 static_cast<int>(blue_down[0]) +
                 static_cast<int>(blue_down[3]) + 2) >>
                2;
            int avg = dst[0];
            dst[0] =
                clampBits((static_cast<int>(blue[0]) - blue_mid) + avg, 16);
            dst[3] =
                clampBits((static_cast<int>(blue[3]) - blue_mid) + avg, 16);
            dst_down[0] = clampBits(
                (static_cast<int>(blue_down[0]) - blue_mid) + avg, 16);
            dst_down[3] = clampBits(
                (static_cast<int>(blue_down[3]) - blue_mid) + avg, 16);
            dst += 6;
            blue += 6;
            blue_down += 6;
            dst_down += 6;
          }
        }
      }
    }
    return;
  } // End if format 30

  if (image.format == 6) {
    for (short &i : curve) {
      i = static_cast<short>(input.getU16());
    }
    max_len = 0;

    struct huff_item {
      uchar8 len;
      uint32 code;
    };

    // FIXME: this probably results in awful padding!
    unique_ptr<struct huff_item[]> huff(new struct huff_item[1024]);

    for (int i = 0; i < 1024; i++) {
      uint32 val = input.getU32();
      huff[i].len = val >> 27;
      huff[i].code = val & 0x7ffffff;
      max_len = max(max_len, val>>27);
    }
    if (max_len>26)
      ThrowRDE("Codelength cannot be longer than 26, invalid data");

    //We create a HUGE table that contains all values up to the
    //maximum code length. Luckily values can only be up to 10
    //bits, so we can get away with using 2 bytes/value
    huge_table = alignedMallocArray<ushort16, 16, ushort16>(1UL << max_len);
    if (!huge_table)
      ThrowRDE("Memory Allocation failed.");

    memset(huge_table, 0xff, (1UL << max_len) * 2);
    for (int i = 0; i < 1024; i++) {
      if (huff[i].len) {
        uint32 len = huff[i].len;
        uint32 code = huff[i].code & ((1 << len) - 1);
        uint32 rem_bits = max_len-len;
        uint32 top_code = (code<<rem_bits);
        ushort16 store_val = (i << 5) | len;
        for (int j = 0; j < (1<<rem_bits); j++)
          huge_table[top_code|j] = store_val;
      }
    }
    // Load offsets
    ByteStream i2(mFile, image.dataOffset + image.dataSize - mRaw->dim.y * 4,
                  static_cast<ByteStream::size_type>(mRaw->dim.y) * 4);
    line_offsets = alignedMallocArray<uint32, 16, uint32>(mRaw->dim.y);
    if (!line_offsets)
      ThrowRDE("Memory Allocation failed.");
    for (int y = 0; y < mRaw->dim.y; y++) {
      line_offsets[y] = i2.getU32() + input.getPosition() + image.dataOffset;
    }
    startThreads();
    return;
  }
  ThrowRDE("Unable to find decoder for format: %d", image.format);
}

void X3fDecoder::createSigmaTable(ByteStream *bytes_, int codes) {
  memset(code_table, 0xff, sizeof(code_table));

  // Read codes and create 8 bit table with all valid values.
  for (int i = 0; i < codes; i++) {
    uint32 len = bytes_->getByte();
    uint32 code = bytes_->getByte();
    if (len > 8)
      ThrowRDE("bit length longer than 8");
    uint32 rem_bits = 8-len;
    for (int j = 0; j < (1<<rem_bits); j++)
      code_table[code|j] = (i << 4) | len;
  }
  // Create a 14 bit table that contains code length
  // AND value. This is enough to decode most images,
  // and will make most codes be returned with a single
  // lookup.
  // If the table value is 0xf, it is not possible to get a
  // value from 14 bits.
  for (int i = 0; i < (1<<14); i++) {
    uint32 top = i>>6;
    uchar8 val = code_table[top];
    if (val != 0xff) {
      uint32 code_bits = val&0xf;
      uint32 val_bits = val>>4;
      if (code_bits + val_bits < 14) {
        uint32 low_pos = 14-code_bits-val_bits;
        int v = (i >> low_pos) & ((1 << val_bits) - 1);
        if (val_bits != 0)
          v = HuffmanTable::signExtended(v, val_bits);
        big_table[i] = (256 * v) | (code_bits + val_bits);
      } else {
        big_table[i] = 0xf;
      }
    } else {
      big_table[i] = 0xf;
    }
  }
}

void X3fDecoder::decodeThreaded( RawDecoderThread* t )
{
  if (curr_image->format == 30 || curr_image->format == 35) {
    uint32 i = t->taskNo;
    assert(i < 3); // see startTasks above

    // Subsampling (in shifts)
    int subs = 0;
    iPoint2D dim = mRaw->dim;
    // Pixels to skip in right side of the image.
    int skipX = 0;
    if (curr_image->format == 35) {
      dim = planeDim[i];
      if (i < 2)
        subs = 1;
      if (dim.x > mRaw->dim.x) {
        skipX = dim.x - mRaw->dim.x;
        dim.x = mRaw->dim.x;
      }
    }

    /* We have a weird prediction which is actually more appropriate for a CFA image */
    BitPumpMSB bits(mFile, plane_offset[i]);
    /* Initialize predictors */
    int pred_up[4];
    int pred_left[2];
    for (int &j : pred_up)
      j = pred[i];

    for (int y = 0; y < dim.y; y++) {
      ushort16* dst =
          reinterpret_cast<ushort16*>(mRaw->getData(0, y << subs)) + i;
      int diff1= SigmaDecode(&bits);
      int diff2 = SigmaDecode(&bits);
      dst[0] = pred_left[0] = pred_up[y & 1] = pred_up[y & 1] + diff1;
      dst[3<<subs] = pred_left[1] = pred_up[(y & 1) + 2] = pred_up[(y & 1) + 2] + diff2;
      dst += 6<<subs;
      // We decode two pixels every loop
      for (int x = 2; x < dim.x; x += 2) {
        diff1 = SigmaDecode(&bits);
        diff2 = SigmaDecode(&bits);
        dst[0] = pred_left[0] = pred_left[0] + diff1;
        dst[3<<subs] = pred_left[1] = pred_left[1] + diff2;
        dst += 6<<subs;
      }
      // If plane is larger than image, skip that number of pixels.
      for (int j = 0; j < skipX; j++)
        SigmaSkipOne(&bits);
    }
    return;
  }

  if (curr_image->format == 6) {
    // FIXME: or does it want X3fDecoder::pred[3] here?
    int predictor[3];
    for (uint32 y = t->start_y; y < t->end_y; y++) {
      BitPumpMSB bits(mFile, line_offsets[y]);
      auto* dst = reinterpret_cast<ushort16*>(mRaw->getData(0, y));
      predictor[0] = predictor[1] = predictor[2] = 0;
      for (int x = 0; x < mRaw->dim.x; x++) {
        for (int &i : predictor) {
          ushort16 val = huge_table[bits.peekBits(max_len)];
          uchar8 nbits = val&31;
          if (val == 0xffff) {
            ThrowRDE("Invalid Huffman value. Image Corrupt");
          }
          bits.skipBitsNoFill(nbits);

          const ushort16 curveElement = val >> 5;
          assert(curveElement < curve.size());
          i += curve[curveElement];
          dst[0] = clampBits(i, 16);
          dst++;
        }
      }
    }
  }
}

/* Skip a single value */
void X3fDecoder::SigmaSkipOne(BitPumpMSB *bits) {
  bits->fill();
  uint32 code = bits->peekBitsNoFill(14);
  int32 bigv = big_table[code];
  if (bigv != 0xf) {
    bits->skipBitsNoFill(bigv&0xff);
    return;
  }
  uchar8 val = code_table[code>>6];
  if (val == 0xff)
    ThrowRDE("Invalid Huffman code");

  uint32 code_bits = val&0xf;
  uint32 val_bits = val>>4;
  bits->skipBitsNoFill(code_bits+val_bits);
}


/* Returns a single value by reading the bitstream*/
int X3fDecoder::SigmaDecode(BitPumpMSB *bits) {

  bits->fill();
  uint32 code = bits->peekBitsNoFill(14);
  int32 bigv = big_table[code];
  if (bigv != 0xf) {
    bits->skipBitsNoFill(bigv&0xff);
    return bigv >> 8;
  }
  uchar8 val = code_table[code>>6];
  if (val == 0xff)
    ThrowRDE("Invalid Huffman code");

  uint32 code_bits = val&0xf;
  uint32 val_bits = val>>4;
  bits->skipBitsNoFill(code_bits);
  if (!val_bits)
    return 0;
  int v = bits->getBitsNoFill(val_bits);
  v = HuffmanTable::signExtended(v, val_bits);

  return v;
}

Buffer* X3fDecoder::getCompressedData() {
  auto img = mImages.begin();
  for (; img !=  mImages.end(); ++img) {
    X3fImage cimg = *img;
    if (cimg.type == 1 || cimg.type == 3) {
      return new Buffer(mFile->getSubView(cimg.dataOffset, cimg.dataSize));
    }
  }
  return nullptr;
}

} // namespace rawspeed
