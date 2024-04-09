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
#include "adt/Point.h"
#include "bitstreams/BitStreams.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/UncompressedDecompressor.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "metadata/Camera.h"
#include "parsers/TiffParser.h"
#include "tiff/TiffIFD.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>

namespace rawspeed {

class CameraMetaData;

MrwDecoder::MrwDecoder(Buffer file) : RawDecoder(file) { parseHeader(); }

int MrwDecoder::isMRW(Buffer input) {
  static const std::array<char, 4> magic = {{0x00, 'M', 'R', 'M'}};
  const Buffer data = input.getSubView(0, magic.size());
  return 0 == memcmp(data.begin(), magic.data(), magic.size());
}

void MrwDecoder::parseHeader() {
  if (!isMRW(mFile))
    ThrowRDE("This isn't actually a MRW file, why are you calling me?");

  const DataBuffer db(mFile, Endianness::big);
  ByteStream bs(db);

  // magic
  bs.skipBytes(4);

  // the size of the rest of the header, up to the image data
  const auto headerSize = bs.getU32();
  (void)bs.check(headerSize);

  // ... and offset to the image data at the same time
  const auto dataOffset = bs.getPosition() + headerSize;
  assert(bs.getPosition() == 8);

  // now, let's parse rest of the header.
  bs = bs.getSubStream(0, dataOffset);
  bs.skipBytes(8);

  bool foundPRD = false;
  while (bs.getRemainSize() > 0) {
    uint32_t tag = bs.getU32();
    uint32_t len = bs.getU32();
    (void)bs.check(len);
    if (!len)
      ThrowRDE("Found entry of zero length, MRW is corrupt.");

    const auto origPos = bs.getPosition();

    switch (tag) {
    case 0x505244: { // PRD
      foundPRD = true;
      bs.skipBytes(8);          // Version Number
      raw_height = bs.getU16(); // CCD Size Y
      raw_width = bs.getU16();  // CCD Size X

      if (!raw_width || !raw_height || raw_width > 3280 || raw_height > 2456) {
        ThrowRDE("Unexpected image dimensions found: (%u; %u)", raw_width,
                 raw_height);
      }

      bs.skipBytes(2); // Image Size Y
      bs.skipBytes(2); // Image Size X

      bpp = bs.getByte(); // DataSize
      if (12 != bpp && 16 != bpp)
        ThrowRDE("Unknown data size");

      if ((raw_height * raw_width * bpp) % 8 != 0)
        ThrowRDE("Bad combination of image size and raw dimensions.");

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
      rootIFD = TiffParser::parse(nullptr, bs.getBuffer(len));
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

  if (!foundPRD)
    ThrowRDE("Did not find PRD tag. Image corrupt.");

  // processed all of the header. the image data is directly next

  const auto imageBits = raw_height * raw_width * bpp;
  assert(imageBits > 0);
  assert(imageBits % 8 == 0);

  imageData = db.getSubView(bs.getPosition(), imageBits / 8);
}

RawImage MrwDecoder::decodeRawInternal() {
  mRaw->dim = iPoint2D(raw_width, raw_height);

  DataBuffer db(imageData, Endianness::big);
  ByteStream bs(db);

  if (packed) {
    UncompressedDecompressor u(
        bs, mRaw, iRectangle2D({0, 0}, iPoint2D(raw_width, raw_height)),
        12 * raw_width / 8, 12, BitOrder::MSB);
    mRaw->createData();
    u.readUncompressedRaw();
  } else {
    UncompressedDecompressor u(
        bs, mRaw, iRectangle2D({0, 0}, iPoint2D(raw_width, raw_height)),
        2 * raw_width, 16, BitOrder::MSB);
    mRaw->createData();
    u.readUncompressedRaw();
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
  // Default
  int iso = 0;

  if (!rootIFD)
    ThrowRDE("Couldn't find make and model");

  auto id = rootIFD->getID();
  setMetaData(meta, id.make, id.model, "", iso);

  if (hints.contains("swapped_wb")) {
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
