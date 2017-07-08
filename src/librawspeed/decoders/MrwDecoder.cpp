/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real
    Copyright (C) 2017 Roman Lebedev

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

#include "decoders/MrwDecoder.h"
#include "common/Common.h"                          // for uint32
#include "common/Point.h"                           // for iPoint2D
#include "decoders/RawDecoderException.h"           // for RawDecoderExcept...
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer, DataBuffer
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for Endianness::big
#include "io/IOException.h"                         // for IOException
#include "metadata/Camera.h"                        // for Hints
#include "parsers/TiffParser.h"                     // for TiffParser
#include "tiff/TiffIFD.h"                           // for TiffID, TiffRoot...
#include <cassert>                                  // for assert
#include <cstring>                                  // for memcmp, size_t
#include <memory>                                   // for unique_ptr

namespace rawspeed {

class CameraMetaData;

MrwDecoder::MrwDecoder(const Buffer* file) : RawDecoder(file) { parseHeader(); }

int MrwDecoder::isMRW(const Buffer* input) {
  static const char magic[] = {0x00, 'M', 'R', 'M'};
  static const size_t magic_size = sizeof(magic);
  static_assert(4 == magic_size, "wrong magic size");

  const unsigned char* data = input->getData(0, magic_size);
  return 0 == memcmp(&data[0], magic, magic_size);
}

void MrwDecoder::parseHeader() {
  if (!isMRW(mFile))
    ThrowRDE("This isn't actually a MRW file, why are you calling me?");

  const DataBuffer db(*mFile, getHostEndianness() == big);
  ByteStream bs(db);

  // magic
  bs.skipBytes(4);

  // the size of the rest of the header, up to the image data
  const auto headerSize = bs.getU32();
  bs.check(headerSize);

  // ... and offset to the image data at the same time
  const auto dataOffset = bs.getPosition() + headerSize;
  assert(bs.getPosition() == 8);

  // now, let's parse rest of the header.
  bs = bs.getSubStream(0, dataOffset);
  bs.skipBytes(8);

  while (bs.getRemainSize() > 0) {
    uint32 tag = bs.getU32();
    uint32 len = bs.getU32();
    bs.check(len);
    if (!len)
      ThrowRDE("Found entry of zero lenght, MRW is corrupt.");

    const auto origPos = bs.getPosition();

    switch (tag) {
    case 0x505244: {            // PRD
      bs.skipBytes(8);          // Version Number
      raw_height = bs.getU16(); // CCD Size Y
      raw_width = bs.getU16();  // CCD Size X
      bs.skipBytes(2);          // Image Size Y
      bs.skipBytes(2);          // Image Size X

      bpp = bs.getByte(); // DataSize
      if (12 != bpp && 16 != bpp)
        ThrowRDE("Unknown data size");

      if (12 != bs.getByte()) // PixelSize
        ThrowRDE("Unexpected pixel size");

      const auto SM = bs.getByte(); // StorageMethod
      if (0x52 != SM && 0x59 != SM)
        ThrowRDE("Unknown storage method");
      packed = (0x59 == SM);

      if ((12 == bpp) != packed)
        ThrowRDE("Packed/BPP sanity check failed!");

      bs.skipBytes(1); // Unknown1
      bs.skipBytes(2); // Unknown2
      bs.skipBytes(2); // BayerPattern
      break;
    }
    case 0x545457: // TTW
      // Base value for offsets needs to be at the beginning of the TIFF block,
      // not the file
      rootIFD = TiffParser::parse(bs.getBuffer(len));
      break;
    case 0x574247:     // WBG
      bs.skipBytes(4); // 4 factors
      static_assert(4 == (sizeof(wb_coeffs) / sizeof(wb_coeffs[0])),
                    "wrong coeff count");
      for (auto& wb_coeff : wb_coeffs)
        wb_coeff = static_cast<float>(bs.getU16()); // gain

      // FIXME?
      // Gf = Gr / 2^(6+F)
      break;
    default:
      // unknown block, let's just ignore
      break;
    }

    bs.setPosition(origPos + len);
  }

  // processed all of the header. the image data is directly next
  const auto imageSize = raw_height * raw_width * bpp / 8;
  imageData = db.getSubView(bs.getPosition(), imageSize);
}

RawImage MrwDecoder::decodeRawInternal() {
  mRaw->dim = iPoint2D(raw_width, raw_height);
  mRaw->createData();

  DataBuffer db(imageData, getHostEndianness() == big);
  ByteStream bs(db);
  UncompressedDecompressor u(bs, mRaw);

  try {
    if (packed)
      u.decode12BitRaw<big>(raw_width, raw_height);
    else
      u.decodeRawUnpacked<12, big>(raw_width, raw_height);
  } catch (IOException &e) {
    mRaw->setError(e.what());
    // Let's ignore it, it may have delivered somewhat useful data.
  }

  return mRaw;
}

void MrwDecoder::checkSupportInternal(const CameraMetaData* meta) {
  if (!rootIFD)
    ThrowRDE("Couldn't find make and model");

  auto id = rootIFD->getID();
  this->checkCameraSupported(meta, id.make, id.model, "");
}

void MrwDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  //Default
  int iso = 0;

  if (!rootIFD)
    ThrowRDE("Couldn't find make and model");

  auto id = rootIFD->getID();
  setMetaData(meta, id.make, id.model, "", iso);

  if (hints.has("swapped_wb")) {
    mRaw->metadata.wbCoeffs[0] = wb_coeffs[2];
    mRaw->metadata.wbCoeffs[1] = wb_coeffs[0];
    mRaw->metadata.wbCoeffs[2] = wb_coeffs[1];
  } else {
    mRaw->metadata.wbCoeffs[0] = wb_coeffs[0];
    mRaw->metadata.wbCoeffs[1] = wb_coeffs[1];
    mRaw->metadata.wbCoeffs[2] = wb_coeffs[3];
  }
}

} // namespace rawspeed
