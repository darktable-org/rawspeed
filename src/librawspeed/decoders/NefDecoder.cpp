/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2015 Pedro Côrte-Real

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

#include "decoders/NefDecoder.h"
#include "common/Common.h"                          // for uint32_t, uint8_t
#include "common/Point.h"                           // for iPoint2D
#include "decoders/RawDecoderException.h"           // for ThrowRDE
#include "decompressors/NikonDecompressor.h"        // for NikonDecompressor
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/BitPumpMSB.h"                          // for BitPumpMSB
#include "io/Buffer.h"                              // for Buffer
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for getU16BE, Endian...
#include "io/IOException.h"                         // for ThrowIOE
#include "metadata/Camera.h"                        // for Hints
#include "metadata/CameraMetaData.h"                // for CameraMetaData
#include "metadata/ColorFilterArray.h"              // for CFA_GREEN, CFA_BLUE
#include "tiff/TiffEntry.h"                         // for TiffEntry, TIFF_...
#include "tiff/TiffIFD.h"                           // for TiffRootIFD, Tif...
#include "tiff/TiffTag.h"                           // for TiffTag, IMAGELE...
#include <algorithm>                                // for min
#include <cassert>                                  // for assert
#include <cmath>                                    // for pow, exp, log
#include <memory>                                   // for unique_ptr, allo...
#include <sstream>                                  // for operator<<, ostr...
#include <string>                                   // for string, operator==
#include <vector>                                   // for vector
// IWYU pragma: no_include <ext/alloc_traits.h>

using std::vector;
using std::string;
using std::min;
using std::ostringstream;

namespace rawspeed {

bool NefDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "NIKON CORPORATION" || make == "NIKON";
}

RawImage NefDecoder::decodeRawInternal() {
  auto raw = mRootIFD->getIFDWithTag(CFAPATTERN);
  auto compression = raw->getEntry(COMPRESSION)->getU32();

  TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);

  if (mRootIFD->getEntryRecursive(MODEL)->getString() == "NIKON D100 ") { /**Sigh**/
    if (!mFile->isValid(offsets->getU32()))
      ThrowRDE("Image data outside of file.");
    if (!D100IsCompressed(offsets->getU32())) {
      DecodeD100Uncompressed();
      return mRaw;
    }
  }

  if (compression == 1 || (hints.has("force_uncompressed")) ||
      NEFIsUncompressed(raw)) {
    DecodeUncompressed();
    return mRaw;
  }

  if (NEFIsUncompressedRGB(raw)) {
    DecodeSNefUncompressed();
    return mRaw;
  }

  if (offsets->count != 1) {
    ThrowRDE("Multiple Strips found: %u", offsets->count);
  }
  if (counts->count != offsets->count) {
    ThrowRDE(
        "Byte count number does not match strip size: count:%u, strips:%u ",
        counts->count, offsets->count);
  }
  if (!mFile->isValid(offsets->getU32(), counts->getU32()))
    ThrowRDE("Invalid strip byte count. File probably truncated.");

  if (34713 != compression)
    ThrowRDE("Unsupported compression");

  uint32_t width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(IMAGELENGTH)->getU32();
  uint32_t bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getU32();

  mRaw->dim = iPoint2D(width, height);

  raw = mRootIFD->getIFDWithTag(static_cast<TiffTag>(0x8c));

  TiffEntry *meta;
  if (raw->hasEntry(static_cast<TiffTag>(0x96))) {
    meta = raw->getEntry(static_cast<TiffTag>(0x96));
  } else {
    meta = raw->getEntry(static_cast<TiffTag>(0x8c)); // Fall back
  }

  ByteStream rawData(
      DataBuffer(mFile->getSubView(offsets->getU32(), counts->getU32()),
                 Endianness::little));

  NikonDecompressor n(mRaw, meta->getData(), bitPerPixel);
  mRaw->createData();
  n.decompress(rawData, uncorrectedRawValues);

  return mRaw;
}

/*
Figure out if a NEF file is compressed.  These fancy heuristics
are only needed for the D100, thanks to a bug in some cameras
that tags all images as "compressed".
*/
bool NefDecoder::D100IsCompressed(uint32_t offset) {
  const uint8_t* test = mFile->getData(offset, 256);
  int i;

  for (i = 15; i < 256; i += 16)
    if (test[i])
      return true;

  return false;
}

/* At least the D810 has a broken firmware that tags uncompressed images
   as if they were compressed. For those cases we set uncompressed mode
   by figuring out that the image is the size of uncompressed packing */
bool NefDecoder::NEFIsUncompressed(const TiffIFD* raw) {
  TiffEntry* counts = raw->getEntry(STRIPBYTECOUNTS);
  uint32_t width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(IMAGELENGTH)->getU32();
  uint32_t bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getU32();

  if (!width || !height || !bitPerPixel)
    return false;

  const auto avaliableInputBytes = counts->getU32(0);
  const auto requiredPixels = iPoint2D(width, height).area();

  // Now, there can be three situations.

  // We might have not enough input to produce the requested image size.
  const uint64_t avaliableInputBits = uint64_t(8) * avaliableInputBytes;
  const auto avaliablePixels = avaliableInputBits / bitPerPixel; // round down!
  if (avaliablePixels < requiredPixels)
    return false;

  // We might have exactly enough input with no padding whatsoever.
  if (avaliablePixels == requiredPixels)
    return true;

  // Or, we might have too much input. And sadly this is the worst case.
  // We can't just accept this. Some *compressed* NEF's also pass this check :(
  // Thus, let's accept *some* *small* padding.
  const auto requiredInputBits = bitPerPixel * requiredPixels;
  const auto requiredInputBytes = roundUpDivision(requiredInputBits, 8);
  // While we might have more *pixels* than needed, it does not nessesairly mean
  // that we have more input *bytes*. We might be off by a few pixels, and with
  // small image dimensions and bpp, we might still be in the same byte.
  assert(avaliableInputBytes >= requiredInputBytes);
  const auto totalPadding = avaliableInputBytes - requiredInputBytes;
  if (totalPadding % height != 0)
    return false; // Inconsistent padding makes no sense here.
  const auto perRowPadding = totalPadding / height;
  return perRowPadding < 16;
}

/* At least the D810 has a broken firmware that tags uncompressed images
   as if they were compressed. For those cases we set uncompressed mode
   by figuring out that the image is the size of uncompressed packing */
bool NefDecoder::NEFIsUncompressedRGB(const TiffIFD* raw) {
  uint32_t byteCount = raw->getEntry(STRIPBYTECOUNTS)->getU32(0);
  uint32_t width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(IMAGELENGTH)->getU32();

  if (byteCount % 3 != 0)
    return false;

  return byteCount / 3 == iPoint2D(width, height).area();
}

void NefDecoder::DecodeUncompressed() {
  auto raw = getIFDWithLargestImage(CFAPATTERN);
  TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);
  uint32_t yPerSlice = raw->getEntry(ROWSPERSTRIP)->getU32();
  uint32_t width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(IMAGELENGTH)->getU32();
  uint32_t bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getU32();

  mRaw->dim = iPoint2D(width, height);

  if (width == 0 || height == 0 || width > 8288 || height > 5520)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  if (counts->count != offsets->count) {
    ThrowRDE("Byte count number does not match strip size: "
             "count:%u, stips:%u ",
             counts->count, offsets->count);
  }

  if (yPerSlice == 0 || yPerSlice > static_cast<uint32_t>(mRaw->dim.y) ||
      roundUpDivision(mRaw->dim.y, yPerSlice) != counts->count) {
    ThrowRDE("Invalid y per slice %u or strip count %u (height = %u)",
             yPerSlice, counts->count, mRaw->dim.y);
  }

  vector<NefSlice> slices;
  slices.reserve(counts->count);
  uint32_t offY = 0;

  for (uint32_t s = 0; s < counts->count; s++) {
    NefSlice slice;
    slice.offset = offsets->getU32(s);
    slice.count = counts->getU32(s);

    if (slice.count < 1)
      ThrowRDE("Slice %u is empty", s);

    if (offY + yPerSlice > height)
      slice.h = height - offY;
    else
      slice.h = yPerSlice;

    offY = min(height, offY + yPerSlice);

    if (!mFile->isValid(slice.offset, slice.count))
      ThrowRDE("Slice offset/count invalid");

    slices.push_back(slice);
  }

  if (slices.empty())
    ThrowRDE("No valid slices found. File probably truncated.");

  assert(height == offY);
  assert(slices.size() == counts->count);

  mRaw->createData();
  if (bitPerPixel == 14 && width*slices[0].h*2 == slices[0].count)
    bitPerPixel = 16; // D3 & D810

  bitPerPixel = hints.get("real_bpp", bitPerPixel);

  switch (bitPerPixel) {
  case 12:
  case 14:
  case 16:
    break;
  default:
    ThrowRDE("Invalid bpp found: %u", bitPerPixel);
  }

  bool bitorder = ! hints.has("msb_override");

  offY = 0;
  for (const NefSlice& slice : slices) {
    ByteStream in(DataBuffer(mFile->getSubView(slice.offset, slice.count),
                             Endianness::little));
    iPoint2D size(width, slice.h);
    iPoint2D pos(0, offY);

    if (hints.has("coolpixmangled")) {
      UncompressedDecompressor u(in, mRaw);
      u.readUncompressedRaw(size, pos, width * bitPerPixel / 8, 12,
                            BitOrder_MSB32);
    } else {
      if (hints.has("coolpixsplit"))
        readCoolpixSplitRaw(in, size, pos, width * bitPerPixel / 8);
      else {
        UncompressedDecompressor u(in, mRaw);
        if (in.getSize() % size.y != 0)
          ThrowRDE("Inconsistent row size");
        const auto inputPitchBytes = in.getSize() / size.y;
        u.readUncompressedRaw(size, pos, inputPitchBytes, bitPerPixel,
                              bitorder ? BitOrder_MSB : BitOrder_LSB);
      }
    }

    offY += slice.h;
  }
}

void NefDecoder::readCoolpixSplitRaw(ByteStream input, const iPoint2D& size,
                                     const iPoint2D& offset, int inputPitch) {
  const Array2DRef<uint16_t> img(mRaw->getU16DataAsUncroppedArray2DRef());

  if (size.y % 2 != 0)
    ThrowRDE("Odd number of rows");
  if (size.x % 8 != 0)
    ThrowRDE("Column count isn't multiple of 8");
  if (inputPitch != ((3 * size.x) / 2))
    ThrowRDE("Unexpected input pitch");

  // BitPumpMSB loads exactly 4 bytes at once, and we squeeze 12 bits each time.
  // We produce 2 pixels per 3 bytes (24 bits). If we want to be smart and to
  // know where the first input bit for first odd row is, the input slice width
  // must be a multiple of 8 pixels.

  if (offset.x > mRaw->dim.x || offset.y > mRaw->dim.y)
    ThrowRDE("All pixels outside of image");
  if (offset.x + size.x > mRaw->dim.x || offset.y + size.y > mRaw->dim.y)
    ThrowRDE("Output is partailly out of image");

  // The input bytes are laid out in the memory in the following way:
  // First, all even (0-2-4-) rows, and then all odd (1-3-5-) rows.
  BitPumpMSB even(input.getStream(size.y / 2, inputPitch));
  BitPumpMSB odd(input.getStream(size.y / 2, inputPitch));
  for (int row = offset.y; row < size.y;) {
    for (int col = offset.x; col < size.x; ++col)
      img(row, col) = even.getBits(12);
    ++row;
    for (int col = offset.x; col < size.x; ++col)
      img(row, col) = odd.getBits(12);
    ++row;
  }
  assert(even.getRemainSize() == 0 && odd.getRemainSize() == 0 &&
         "Should have run out of input");
}

void NefDecoder::DecodeD100Uncompressed() {
  auto ifd = mRootIFD->getIFDWithTag(STRIPOFFSETS, 1);

  uint32_t offset = ifd->getEntry(STRIPOFFSETS)->getU32();
  // Hardcode the sizes as at least the width is not correctly reported
  uint32_t width = 3040;
  uint32_t height = 2024;

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  ByteStream bs(DataBuffer(mFile->getSubView(offset), Endianness::little));
  if (bs.getRemainSize() == 0)
    ThrowRDE("No input to decode!");

  UncompressedDecompressor u(
      ByteStream(DataBuffer(mFile->getSubView(offset), Endianness::little)),
      mRaw);

  u.decode12BitRaw<Endianness::big, false, true>(width, height);
}

void NefDecoder::DecodeSNefUncompressed() {
  auto raw = getIFDWithLargestImage(CFAPATTERN);
  uint32_t offset = raw->getEntry(STRIPOFFSETS)->getU32();
  uint32_t width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32_t height = raw->getEntry(IMAGELENGTH)->getU32();

  if (width == 0 || height == 0 || width % 2 != 0 || width > 3680 ||
      height > 2456)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  mRaw->dim = iPoint2D(width, height);
  mRaw->setCpp(3);
  mRaw->isCFA = false;
  mRaw->createData();

  ByteStream in(DataBuffer(mFile->getSubView(offset), Endianness::little));
  DecodeNikonSNef(in);
}

void NefDecoder::checkSupportInternal(const CameraMetaData* meta) {
  auto id = mRootIFD->getID();
  string mode = getMode();
  string extended_mode = getExtendedMode(mode);

  if (meta->hasCamera(id.make, id.model, extended_mode))
    checkCameraSupported(meta, id, extended_mode);
  else
    checkCameraSupported(meta, id, mode);
}

string NefDecoder::getMode() {
  ostringstream mode;
  auto raw = getIFDWithLargestImage(CFAPATTERN);
  int compression = raw->getEntry(COMPRESSION)->getU32();
  uint32_t bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getU32();

  if (NEFIsUncompressedRGB(raw))
    mode << "sNEF-uncompressed";
  else {
    if (1 == compression || NEFIsUncompressed(raw))
      mode << bitPerPixel << "bit-uncompressed";
    else
      mode << bitPerPixel << "bit-compressed";
  }
  return mode.str();
}

string NefDecoder::getExtendedMode(const string &mode) {
  ostringstream extended_mode;

  auto ifd = mRootIFD->getIFDWithTag(CFAPATTERN);
  uint32_t width = ifd->getEntry(IMAGEWIDTH)->getU32();
  uint32_t height = ifd->getEntry(IMAGELENGTH)->getU32();

  extended_mode << width << "x" << height << "-" << mode;
  return extended_mode.str();
}

// We use this for the D50 and D2X whacky WB "encryption"
const std::array<uint8_t, 256> NefDecoder::serialmap = {
    {0xc1, 0xbf, 0x6d, 0x0d, 0x59, 0xc5, 0x13, 0x9d, 0x83, 0x61, 0x6b, 0x4f,
     0xc7, 0x7f, 0x3d, 0x3d, 0x53, 0x59, 0xe3, 0xc7, 0xe9, 0x2f, 0x95, 0xa7,
     0x95, 0x1f, 0xdf, 0x7f, 0x2b, 0x29, 0xc7, 0x0d, 0xdf, 0x07, 0xef, 0x71,
     0x89, 0x3d, 0x13, 0x3d, 0x3b, 0x13, 0xfb, 0x0d, 0x89, 0xc1, 0x65, 0x1f,
     0xb3, 0x0d, 0x6b, 0x29, 0xe3, 0xfb, 0xef, 0xa3, 0x6b, 0x47, 0x7f, 0x95,
     0x35, 0xa7, 0x47, 0x4f, 0xc7, 0xf1, 0x59, 0x95, 0x35, 0x11, 0x29, 0x61,
     0xf1, 0x3d, 0xb3, 0x2b, 0x0d, 0x43, 0x89, 0xc1, 0x9d, 0x9d, 0x89, 0x65,
     0xf1, 0xe9, 0xdf, 0xbf, 0x3d, 0x7f, 0x53, 0x97, 0xe5, 0xe9, 0x95, 0x17,
     0x1d, 0x3d, 0x8b, 0xfb, 0xc7, 0xe3, 0x67, 0xa7, 0x07, 0xf1, 0x71, 0xa7,
     0x53, 0xb5, 0x29, 0x89, 0xe5, 0x2b, 0xa7, 0x17, 0x29, 0xe9, 0x4f, 0xc5,
     0x65, 0x6d, 0x6b, 0xef, 0x0d, 0x89, 0x49, 0x2f, 0xb3, 0x43, 0x53, 0x65,
     0x1d, 0x49, 0xa3, 0x13, 0x89, 0x59, 0xef, 0x6b, 0xef, 0x65, 0x1d, 0x0b,
     0x59, 0x13, 0xe3, 0x4f, 0x9d, 0xb3, 0x29, 0x43, 0x2b, 0x07, 0x1d, 0x95,
     0x59, 0x59, 0x47, 0xfb, 0xe5, 0xe9, 0x61, 0x47, 0x2f, 0x35, 0x7f, 0x17,
     0x7f, 0xef, 0x7f, 0x95, 0x95, 0x71, 0xd3, 0xa3, 0x0b, 0x71, 0xa3, 0xad,
     0x0b, 0x3b, 0xb5, 0xfb, 0xa3, 0xbf, 0x4f, 0x83, 0x1d, 0xad, 0xe9, 0x2f,
     0x71, 0x65, 0xa3, 0xe5, 0x07, 0x35, 0x3d, 0x0d, 0xb5, 0xe9, 0xe5, 0x47,
     0x3b, 0x9d, 0xef, 0x35, 0xa3, 0xbf, 0xb3, 0xdf, 0x53, 0xd3, 0x97, 0x53,
     0x49, 0x71, 0x07, 0x35, 0x61, 0x71, 0x2f, 0x43, 0x2f, 0x11, 0xdf, 0x17,
     0x97, 0xfb, 0x95, 0x3b, 0x7f, 0x6b, 0xd3, 0x25, 0xbf, 0xad, 0xc7, 0xc5,
     0xc5, 0xb5, 0x8b, 0xef, 0x2f, 0xd3, 0x07, 0x6b, 0x25, 0x49, 0x95, 0x25,
     0x49, 0x6d, 0x71, 0xc7}};
const std::array<uint8_t, 256> NefDecoder::keymap = {
    {0xa7, 0xbc, 0xc9, 0xad, 0x91, 0xdf, 0x85, 0xe5, 0xd4, 0x78, 0xd5, 0x17,
     0x46, 0x7c, 0x29, 0x4c, 0x4d, 0x03, 0xe9, 0x25, 0x68, 0x11, 0x86, 0xb3,
     0xbd, 0xf7, 0x6f, 0x61, 0x22, 0xa2, 0x26, 0x34, 0x2a, 0xbe, 0x1e, 0x46,
     0x14, 0x68, 0x9d, 0x44, 0x18, 0xc2, 0x40, 0xf4, 0x7e, 0x5f, 0x1b, 0xad,
     0x0b, 0x94, 0xb6, 0x67, 0xb4, 0x0b, 0xe1, 0xea, 0x95, 0x9c, 0x66, 0xdc,
     0xe7, 0x5d, 0x6c, 0x05, 0xda, 0xd5, 0xdf, 0x7a, 0xef, 0xf6, 0xdb, 0x1f,
     0x82, 0x4c, 0xc0, 0x68, 0x47, 0xa1, 0xbd, 0xee, 0x39, 0x50, 0x56, 0x4a,
     0xdd, 0xdf, 0xa5, 0xf8, 0xc6, 0xda, 0xca, 0x90, 0xca, 0x01, 0x42, 0x9d,
     0x8b, 0x0c, 0x73, 0x43, 0x75, 0x05, 0x94, 0xde, 0x24, 0xb3, 0x80, 0x34,
     0xe5, 0x2c, 0xdc, 0x9b, 0x3f, 0xca, 0x33, 0x45, 0xd0, 0xdb, 0x5f, 0xf5,
     0x52, 0xc3, 0x21, 0xda, 0xe2, 0x22, 0x72, 0x6b, 0x3e, 0xd0, 0x5b, 0xa8,
     0x87, 0x8c, 0x06, 0x5d, 0x0f, 0xdd, 0x09, 0x19, 0x93, 0xd0, 0xb9, 0xfc,
     0x8b, 0x0f, 0x84, 0x60, 0x33, 0x1c, 0x9b, 0x45, 0xf1, 0xf0, 0xa3, 0x94,
     0x3a, 0x12, 0x77, 0x33, 0x4d, 0x44, 0x78, 0x28, 0x3c, 0x9e, 0xfd, 0x65,
     0x57, 0x16, 0x94, 0x6b, 0xfb, 0x59, 0xd0, 0xc8, 0x22, 0x36, 0xdb, 0xd2,
     0x63, 0x98, 0x43, 0xa1, 0x04, 0x87, 0x86, 0xf7, 0xa6, 0x26, 0xbb, 0xd6,
     0x59, 0x4d, 0xbf, 0x6a, 0x2e, 0xaa, 0x2b, 0xef, 0xe6, 0x78, 0xb6, 0x4e,
     0xe0, 0x2f, 0xdc, 0x7c, 0xbe, 0x57, 0x19, 0x32, 0x7e, 0x2a, 0xd0, 0xb8,
     0xba, 0x29, 0x00, 0x3c, 0x52, 0x7d, 0xa8, 0x49, 0x3b, 0x2d, 0xeb, 0x25,
     0x49, 0xfa, 0xa3, 0xaa, 0x39, 0xa7, 0xc5, 0xa7, 0x50, 0x11, 0x36, 0xfb,
     0xc6, 0x67, 0x4a, 0xf5, 0xa5, 0x12, 0x65, 0x7e, 0xb0, 0xdf, 0xaf, 0x4e,
     0xb3, 0x61, 0x7f, 0x2f}};

void NefDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  int iso = 0;
  mRaw->cfa.setCFA(iPoint2D(2,2), CFA_RED, CFA_GREEN, CFA_GREEN, CFA_BLUE);

  int white = mRaw->whitePoint;
  int black = mRaw->blackLevel;

  if (mRootIFD->hasEntryRecursive(ISOSPEEDRATINGS))
    iso = mRootIFD->getEntryRecursive(ISOSPEEDRATINGS)->getU32();

  // Read the whitebalance

  if (mRootIFD->hasEntryRecursive(static_cast<TiffTag>(12))) {
    TiffEntry* wb = mRootIFD->getEntryRecursive(static_cast<TiffTag>(12));
    if (wb->count == 4) {
      mRaw->metadata.wbCoeffs[0] = wb->getFloat(0);
      mRaw->metadata.wbCoeffs[1] = wb->getFloat(2);
      mRaw->metadata.wbCoeffs[2] = wb->getFloat(1);
      if (mRaw->metadata.wbCoeffs[1] <= 0.0F)
        mRaw->metadata.wbCoeffs[1] = 1.0F;
    }
  } else if (mRootIFD->hasEntryRecursive(static_cast<TiffTag>(0x0097))) {
    TiffEntry* wb = mRootIFD->getEntryRecursive(static_cast<TiffTag>(0x0097));
    if (wb->count > 4) {
      uint32_t version = 0;
      for (uint32_t i = 0; i < 4; i++) {
        const auto v = wb->getByte(i);
        if (v < '0' || v > '9')
          ThrowRDE("Bad version component: %c - not a digit", v);
        version = (version << 4) + v - '0';
      }

      if (version == 0x100 && wb->count >= 80 && wb->type == TIFF_UNDEFINED) {
        mRaw->metadata.wbCoeffs[0] = static_cast<float>(wb->getU16(36));
        mRaw->metadata.wbCoeffs[2] = static_cast<float>(wb->getU16(37));
        mRaw->metadata.wbCoeffs[1] = static_cast<float>(wb->getU16(38));
      } else if (version == 0x103 && wb->count >= 26 && wb->type == TIFF_UNDEFINED) {
        mRaw->metadata.wbCoeffs[0] = static_cast<float>(wb->getU16(10));
        mRaw->metadata.wbCoeffs[1] = static_cast<float>(wb->getU16(11));
        mRaw->metadata.wbCoeffs[2] = static_cast<float>(wb->getU16(12));
      } else if (((version == 0x204 && wb->count >= 564) ||
                  (version == 0x205 && wb->count >= 284)) &&
                 mRootIFD->hasEntryRecursive(static_cast<TiffTag>(0x001d)) &&
                 mRootIFD->hasEntryRecursive(static_cast<TiffTag>(0x00a7))) {
        // Get the serial number
        string serial =
            mRootIFD->getEntryRecursive(static_cast<TiffTag>(0x001d))
                ->getString();
        if (serial.length() > 9)
          ThrowRDE("Serial number is too long (%zu)", serial.length());
        uint32_t serialno = 0;
        for (unsigned char c : serial) {
          if (c >= '0' && c <= '9')
            serialno = serialno*10 + c-'0';
          else
            serialno = serialno*10 + c%10;
        }

        // Get the decryption key
        TiffEntry* key =
            mRootIFD->getEntryRecursive(static_cast<TiffTag>(0x00a7));
        const uint8_t* keydata = key->getData(4);
        uint32_t keyno = keydata[0] ^ keydata[1] ^ keydata[2] ^ keydata[3];

        // "Decrypt" the block using the serial and key
        uint8_t ci = serialmap[serialno & 0xff];
        uint8_t cj = keymap[keyno & 0xff];
        uint8_t ck = 0x60;

        ByteStream bs = wb->getData();
        bs.skipBytes(version == 0x204 ? 284 : 4);

        std::array<uint8_t, 14 + 8> buf;
        for (unsigned char& i : buf) {
          cj = uint8_t(cj + ci * ck); // modulo arithmetics.
          i = bs.getByte() ^ cj;
          ck++;
        }

        // Finally set the WB coeffs
        uint32_t off = (version == 0x204) ? 6 : 14;
        mRaw->metadata.wbCoeffs[0] =
            static_cast<float>(getU16BE(buf.data() + off + 0));
        mRaw->metadata.wbCoeffs[1] =
            static_cast<float>(getU16BE(buf.data() + off + 2));
        mRaw->metadata.wbCoeffs[2] =
            static_cast<float>(getU16BE(buf.data() + off + 6));
      }
    }
  } else if (mRootIFD->hasEntryRecursive(static_cast<TiffTag>(0x0014))) {
    TiffEntry* wb = mRootIFD->getEntryRecursive(static_cast<TiffTag>(0x0014));
    ByteStream bs = wb->getData();
    if (wb->count == 2560 && wb->type == TIFF_UNDEFINED) {
      bs.skipBytes(1248);
      bs.setByteOrder(Endianness::big);
      mRaw->metadata.wbCoeffs[0] = static_cast<float>(bs.getU16()) / 256.0;
      mRaw->metadata.wbCoeffs[1] = 1.0F;
      mRaw->metadata.wbCoeffs[2] = static_cast<float>(bs.getU16()) / 256.0;
    } else if (bs.hasPatternAt("NRW ", 4, 0)) {
      uint32_t offset = 0;
      if (!bs.hasPatternAt("0100", 4, 4) && wb->count > 72)
        offset = 56;
      else if (wb->count > 1572)
        offset = 1556;

      if (offset) {
        bs.skipBytes(offset);
        bs.setByteOrder(Endianness::little);
        mRaw->metadata.wbCoeffs[0] = 4.0 * bs.getU32();
        mRaw->metadata.wbCoeffs[1] = bs.getU32();
        mRaw->metadata.wbCoeffs[1] += bs.getU32();
        mRaw->metadata.wbCoeffs[2] = 4.0 * bs.getU32();
      }
    }
  }

  if (hints.has("nikon_wb_adjustment")) {
    mRaw->metadata.wbCoeffs[0] *= 256/527.0;
    mRaw->metadata.wbCoeffs[2] *= 256/317.0;
  }

  auto id = mRootIFD->getID();
  string mode = getMode();
  string extended_mode = getExtendedMode(mode);
  if (meta->hasCamera(id.make, id.model, extended_mode)) {
    setMetaData(meta, id, extended_mode, iso);
  } else if (meta->hasCamera(id.make, id.model, mode)) {
    setMetaData(meta, id, mode, iso);
  } else {
    setMetaData(meta, id, "", iso);
  }

  if (white != 65536)
    mRaw->whitePoint = white;
  if (black != -1)
    mRaw->blackLevel = black;
}


// DecodeNikonYUY2 decodes 12 bit data in an YUY2-like pattern (2 Luma, 1 Chroma per 2 pixels).
// We un-apply the whitebalance, so output matches lossless.
// Note that values are scaled. See comment below on details.
// OPTME: It would be trivial to run this multithreaded.
void NefDecoder::DecodeNikonSNef(const ByteStream& input) {
  if (mRaw->dim.x < 6)
    ThrowIOE("got a %u wide sNEF, aborting", mRaw->dim.x);

  // We need to read the applied whitebalance, since we should return
  // data before whitebalance, so we "unapply" it.
  TiffEntry* wb = mRootIFD->getEntryRecursive(static_cast<TiffTag>(12));
  if (!wb)
    ThrowRDE("Unable to locate whitebalance needed for decompression");

  assert(wb != nullptr);
  if (wb->count != 4 || wb->type != TIFF_RATIONAL)
    ThrowRDE("Whitebalance has unknown count or type");

  float wb_r = wb->getFloat(0);
  float wb_b = wb->getFloat(1);

  // ((1024/x)*((1<<16)-1)+(1<<9))<=((1<<31)-1), x>0  gives: (0.0312495)
  const float lower_limit = 13'421'568.0 / 429'496'627.0;
  if (wb_r < lower_limit || wb_b < lower_limit || wb_r > 10.0F || wb_b > 10.0F)
    ThrowRDE("Whitebalance has bad values (%f, %f)", wb_r, wb_b);

  mRaw->metadata.wbCoeffs[0] = wb_r;
  mRaw->metadata.wbCoeffs[1] = 1.0F;
  mRaw->metadata.wbCoeffs[2] = wb_b;

  auto inv_wb_r = static_cast<int>(1024.0 / wb_r);
  auto inv_wb_b = static_cast<int>(1024.0 / wb_b);

  auto curve = gammaCurve(1 / 2.4, 12.92, 1, 4095);

  // Scale output values to 16 bits.
  for (int i = 0 ; i < 4096; i++) {
    curve[i] = clampBits(static_cast<int>(curve[i]) << 2, 16);
  }

  curve.resize(4095);

  RawImageCurveGuard curveHandler(&mRaw, curve, false);

  uint16_t tmp;
  auto* tmpch = reinterpret_cast<uint8_t*>(&tmp);

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  const uint8_t* in = input.peekData(out.width * out.height);

  for (int row = 0; row < out.height; row++) {
    uint32_t random = in[0] + (in[1] << 8) + (in[2] << 16);
    for (int col = 0; col < out.width; col += 6) {
      uint32_t g1 = in[0];
      uint32_t g2 = in[1];
      uint32_t g3 = in[2];
      uint32_t g4 = in[3];
      uint32_t g5 = in[4];
      uint32_t g6 = in[5];

      in+=6;
      auto y1 = static_cast<float>(g1 | ((g2 & 0x0f) << 8));
      auto y2 = static_cast<float>((g2 >> 4) | (g3 << 4));
      auto cb = static_cast<float>(g4 | ((g5 & 0x0f) << 8));
      auto cr = static_cast<float>((g5 >> 4) | (g6 << 4));

      float cb2 = cb;
      float cr2 = cr;
      // Interpolate right pixel. We assume the sample is aligned with left pixel.
      if ((col + 6) < out.width) {
        g4 = in[3];
        g5 = in[4];
        g6 = in[5];
        cb2 = (static_cast<float>((g4 | ((g5 & 0x0f) << 8))) + cb) * 0.5F;
        cr2 = (static_cast<float>(((g5 >> 4) | (g6 << 4))) + cr) * 0.5F;
      }

      cb -= 2048;
      cr -= 2048;
      cb2 -= 2048;
      cr2 -= 2048;

      mRaw->setWithLookUp(clampBits(static_cast<int>(y1 + 1.370705 * cr), 12),
                          tmpch, &random);
      out(row, col) = clampBits((inv_wb_r * tmp + (1 << 9)) >> 10, 15);

      mRaw->setWithLookUp(
          clampBits(static_cast<int>(y1 - 0.337633 * cb - 0.698001 * cr), 12),
          reinterpret_cast<uint8_t*>(&out(row, col + 1)), &random);

      mRaw->setWithLookUp(clampBits(static_cast<int>(y1 + 1.732446 * cb), 12),
                          tmpch, &random);
      out(row, col + 2) = clampBits((inv_wb_b * tmp + (1 << 9)) >> 10, 15);

      mRaw->setWithLookUp(clampBits(static_cast<int>(y2 + 1.370705 * cr2), 12),
                          tmpch, &random);
      out(row, col + 3) = clampBits((inv_wb_r * tmp + (1 << 9)) >> 10, 15);

      mRaw->setWithLookUp(
          clampBits(static_cast<int>(y2 - 0.337633 * cb2 - 0.698001 * cr2), 12),
          reinterpret_cast<uint8_t*>(&out(row, col + 4)), &random);

      mRaw->setWithLookUp(clampBits(static_cast<int>(y2 + 1.732446 * cb2), 12),
                          tmpch, &random);
      out(row, col + 5) = clampBits((inv_wb_b * tmp + (1 << 9)) >> 10, 15);
    }
  }
}

// From:  dcraw.c -- Dave Coffin's raw photo decoder
#define SQR(x) ((x)*(x))
std::vector<uint16_t> NefDecoder::gammaCurve(double pwr, double ts, int mode,
                                             int imax) {
  std::vector<uint16_t> curve(65536);

  int i;
  std::array<double, 6> g;
  std::array<double, 2> bnd = {{}};
  double r;
  g[0] = pwr;
  g[1] = ts;
  g[2] = g[3] = g[4] = 0;
  bnd[g[1] >= 1] = 1;
  if (g[1] && (g[1]-1)*(g[0]-1) <= 0) {
    for (i=0; i < 48; i++) {
      g[2] = (bnd[0] + bnd[1])/2;
      if (g[0])
        bnd[(pow(g[2] / g[1], -g[0]) - 1) / g[0] - 1 / g[2] > -1] = g[2];
      else
        bnd[g[2] / exp(1 - 1 / g[2]) < g[1]] = g[2];
    }
    g[3] = g[2] / g[1];
    if (g[0])
      g[4] = g[2] * (1 / g[0] - 1);
  }
  if (g[0]) {
    g[5] = 1 / (g[1] * SQR(g[3]) / 2 - g[4] * (1 - g[3]) +
                (1 - pow(g[3], 1 + g[0])) * (1 + g[4]) / (1 + g[0])) -
           1;
  } else {
    g[5] = 1 / (g[1] * SQR(g[3]) / 2 + 1 - g[2] - g[3] -
                g[2] * g[3] * (log(g[3]) - 1)) -
           1;
  }

  if (mode == 0)
    ThrowRDE("Unimplemented mode");

  mode--;

  for (i=0; i < 0x10000; i++) {
    curve[i] = 0xffff;
    if ((r = static_cast<double>(i) / imax) < 1) {
      curve[i] = static_cast<uint16_t>(
          0x10000 *
          (mode ? (r < g[3] ? r * g[1]
                            : (g[0] ? pow(r, g[0]) * (1 + g[4]) - g[4]
                                    : log(r) * g[2] + 1))
                : (r < g[2] ? r / g[1]
                            : (g[0] ? pow((r + g[4]) / (1 + g[4]), 1 / g[0])
                                    : exp((r - 1) / g[2])))));
    }
  }

  assert(curve.size() == 65536);

  return curve;
}
#undef SQR

} // namespace rawspeed
