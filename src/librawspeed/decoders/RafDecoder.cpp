/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014-2015 Pedro Côrte-Real

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

#include "decoders/RafDecoder.h"
#include "common/Common.h"                          // for uint32, ushort16
#include "common/Point.h"                           // for iPoint2D, iRecta...
#include "decoders/RawDecoderException.h"           // for ThrowRDE
#include "decompressors/FujiDecompressor.h"         // for FujiDecompressor
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for Endianness, getH...
#include "metadata/Camera.h"                        // for Camera, Hints
#include "metadata/CameraMetaData.h"                // for CameraMetaData
#include "metadata/CameraSensorInfo.h"              // for CameraSensorInfo
#include "metadata/ColorFilterArray.h"              // for ColorFilterArray
#include "tiff/TiffEntry.h"                         // for TiffEntry
#include "tiff/TiffIFD.h"                           // for TiffRootIFD, Tif...
#include "tiff/TiffTag.h"                           // for TiffTag::FUJI_RA...
#include <cassert>                                  // for assert
#include <cstdio>                                   // for size_t
#include <cstring>                                  // for memcmp
#include <memory>                                   // for unique_ptr
#include <string>                                   // for string, operator==
#include <vector>                                   // for vector

namespace rawspeed {

bool RafDecoder::isRAF(const Buffer* input) {
  static const char magic[] = "FUJIFILMCCD-RAW ";
  static const size_t magic_size = sizeof(magic) - 1; // excluding \0
  const unsigned char* data = input->getData(0, magic_size);
  return 0 == memcmp(&data[0], magic, magic_size);
}

bool RafDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "FUJIFILM";
}

RawImage RafDecoder::decodeRawInternal() {
  auto raw = mRootIFD->getIFDWithTag(FUJI_STRIPOFFSETS);
  uint32 height = 0;
  uint32 width = 0;

  if (raw->hasEntry(FUJI_RAWIMAGEFULLHEIGHT)) {
    height = raw->getEntry(FUJI_RAWIMAGEFULLHEIGHT)->getU32();
    width = raw->getEntry(FUJI_RAWIMAGEFULLWIDTH)->getU32();
  } else if (raw->hasEntry(IMAGEWIDTH)) {
    TiffEntry *e = raw->getEntry(IMAGEWIDTH);
    height = e->getU16(0);
    width = e->getU16(1);
  } else
    ThrowRDE("Unable to locate image size");

  if (width == 0 || height == 0 || width > 9216 || height > 6210)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  if (raw->hasEntry(FUJI_LAYOUT)) {
    TiffEntry *e = raw->getEntry(FUJI_LAYOUT);
    alt_layout = !(e->getByte(0) >> 7);
  }

  TiffEntry *offsets = raw->getEntry(FUJI_STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(FUJI_STRIPBYTECOUNTS);

  if (offsets->count != 1 || counts->count != 1)
    ThrowRDE("Multiple Strips found: %u %u", offsets->count, counts->count);

  ByteStream input(offsets->getRootIfdData());
  input = input.getSubStream(offsets->getU32(), counts->getU32());

  if (isCompressed()) {
    mRaw->metadata.mode = "compressed";

    mRaw->dim = iPoint2D(width, height);

    FujiDecompressor f(mRaw, input);

    const iPoint2D hDim(f.header.raw_width, f.header.raw_height);

    if (mRaw->dim != hDim)
      ThrowRDE("RAF header specifies different dimensions!");

    mRaw->createData();

    f.decompress();

    return mRaw;
  }

  // x-trans sensors report 14bpp, but data isn't packed
  // thus, unless someone has any better ideas, let's autodetect it.
  int bps;

  // Some fuji SuperCCD cameras include a second raw image next to the first one
  // that is identical but darker to the first. The two combined can produce
  // a higher dynamic range image. Right now we're ignoring it.
  bool double_width;

  assert(!isCompressed());

  if (8UL * counts->getU32() >= 2UL * 16UL * width * height) {
    bps = 16;
    double_width = true;
  } else if (8UL * counts->getU32() >= 2UL * 14UL * width * height) {
    bps = 14;
    double_width = true;
  } else if (8UL * counts->getU32() >= 2UL * 12UL * width * height) {
    bps = 12;
    double_width = true;
  } else if (8UL * counts->getU32() >= 16UL * width * height) {
    bps = 16;
    double_width = false;
  } else if (8UL * counts->getU32() >= 14UL * width * height) {
    bps = 14;
    double_width = false;
  } else if (8UL * counts->getU32() >= 12UL * width * height) {
    bps = 12;
    double_width = false;
  } else {
    ThrowRDE("Can not detect bitdepth. StripByteCounts = %u, width = %u, "
             "height = %u",
             counts->getU32(), width, height);
  }

  double_width = hints.has("double_width_unpacked");
  const uint32 real_width = double_width ? 2U * width : width;

  mRaw->dim = iPoint2D(real_width, height);
  mRaw->createData();

  UncompressedDecompressor u(input, mRaw);

  if (double_width) {
    u.decodeRawUnpacked<16, Endianness::little>(width * 2, height);
  } else if (input.getByteOrder() == Endianness::big &&
             getHostEndianness() == Endianness::little) {
    // FIXME: ^ that if seems fishy
    u.decodeRawUnpacked<16, Endianness::big>(width, height);
  } else {
    iPoint2D pos(0, 0);
    if (hints.has("jpeg32_bitorder")) {
      u.readUncompressedRaw(mRaw->dim, pos, width * bps / 8, bps,
                            BitOrder_MSB32);
    } else {
      u.readUncompressedRaw(mRaw->dim, pos, width * bps / 8, bps, BitOrder_LSB);
    }
  }

  return mRaw;
}

void RafDecoder::checkSupportInternal(const CameraMetaData* meta) {
  if (!this->checkCameraSupported(meta, mRootIFD->getID(), ""))
    ThrowRDE("Unknown camera. Will not guess.");

  if (isCompressed()) {
    mRaw->metadata.mode = "compressed";

    auto id = mRootIFD->getID();
    const Camera* cam = meta->getCamera(id.make, id.model, mRaw->metadata.mode);
    if (!cam)
      ThrowRDE("Couldn't find camera %s %s", id.make.c_str(), id.model.c_str());

    mRaw->cfa = cam->cfa;
  }
}

void RafDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  int iso = 0;
  if (mRootIFD->hasEntryRecursive(ISOSPEEDRATINGS))
    iso = mRootIFD->getEntryRecursive(ISOSPEEDRATINGS)->getU32();
  mRaw->metadata.isoSpeed = iso;

  // This is where we'd normally call setMetaData but since we may still need
  // to rotate the image for SuperCCD cameras we do everything ourselves
  auto id = mRootIFD->getID();
  const Camera* cam = meta->getCamera(id.make, id.model, mRaw->metadata.mode);
  if (!cam)
    ThrowRDE("Couldn't find camera");

  assert(cam != nullptr);

  iPoint2D new_size(mRaw->dim);
  iPoint2D crop_offset = iPoint2D(0,0);

  if (applyCrop) {
    new_size = cam->cropSize;
    crop_offset = cam->cropPos;
    bool double_width = hints.has("double_width_unpacked");
    // If crop size is negative, use relative cropping
    if (new_size.x <= 0)
      new_size.x = mRaw->dim.x / (double_width ? 2 : 1) - cam->cropPos.x + new_size.x;
    else
      new_size.x /= (double_width ? 2 : 1);
    if (new_size.y <= 0)
      new_size.y = mRaw->dim.y - cam->cropPos.y + new_size.y;
  }

  bool rotate = hints.has("fuji_rotate");
  rotate = rotate && fujiRotate;

  // Rotate 45 degrees - could be multithreaded.
  if (rotate && !this->uncorrectedRawValues) {
    // Calculate the 45 degree rotated size;
    uint32 rotatedsize;
    uint32 rotationPos;
    if (alt_layout) {
      rotatedsize = new_size.y+new_size.x/2;
      rotationPos = new_size.x/2 - 1;
    }
    else {
      rotatedsize = new_size.x+new_size.y/2;
      rotationPos = new_size.x - 1;
    }

    iPoint2D final_size(rotatedsize, rotatedsize-1);
    RawImage rotated = RawImage::create(final_size, TYPE_USHORT16, 1);
    rotated->clearArea(iRectangle2D(iPoint2D(0,0), rotated->dim));
    rotated->metadata = mRaw->metadata;
    rotated->metadata.fujiRotationPos = rotationPos;

    int dest_pitch = static_cast<int>(rotated->pitch) / 2;
    auto* dst = reinterpret_cast<ushort16*>(rotated->getData(0, 0));

    for (int y = 0; y < new_size.y; y++) {
      auto* src = reinterpret_cast<ushort16*>(
          mRaw->getData(crop_offset.x, crop_offset.y + y));
      for (int x = 0; x < new_size.x; x++) {
        int h;
        int w;
        if (alt_layout) { // Swapped x and y
          h = rotatedsize - (new_size.y + 1 - y + (x >> 1));
          w = ((x+1) >> 1) + y;
        } else {
          h = new_size.x - 1 - x + (y >> 1);
          w = ((y+1) >> 1) + x;
        }
        if (h < rotated->dim.y && w < rotated->dim.x)
          dst[w + h * dest_pitch] = src[x];
        else
          ThrowRDE("Trying to write out of bounds");
      }
    }
    mRaw = rotated;
  } else if (applyCrop) {
    mRaw->subFrame(iRectangle2D(crop_offset, new_size));
  }

  const CameraSensorInfo *sensor = cam->getSensorInfo(iso);
  mRaw->blackLevel = sensor->mBlackLevel;

  // at least the (bayer sensor) X100 comes with a tag like this:
  if (mRootIFD->hasEntryRecursive(FUJI_BLACKLEVEL)) {
    TiffEntry* sep_black = mRootIFD->getEntryRecursive(FUJI_BLACKLEVEL);
    if (sep_black->count == 4)
    {
      for(int k=0;k<4;k++)
        mRaw->blackLevelSeparate[k] = sep_black->getU32(k);
    } else if (sep_black->count == 36) {
      for (int& k : mRaw->blackLevelSeparate)
        k = 0;

      for (int y = 0; y < 6; y++) {
        for (int x = 0; x < 6; x++)
          mRaw->blackLevelSeparate[2 * (y % 2) + (x % 2)] +=
              sep_black->getU32(6 * y + x);
      }

      for (int& k : mRaw->blackLevelSeparate)
        k /= 9;
    }
  }

  mRaw->whitePoint = sensor->mWhiteLevel;
  mRaw->blackAreas = cam->blackAreas;
  mRaw->cfa = cam->cfa;
  mRaw->metadata.canonical_make = cam->canonical_make;
  mRaw->metadata.canonical_model = cam->canonical_model;
  mRaw->metadata.canonical_alias = cam->canonical_alias;
  mRaw->metadata.canonical_id = cam->canonical_id;
  mRaw->metadata.make = id.make;
  mRaw->metadata.model = id.model;

  if (mRootIFD->hasEntryRecursive(FUJI_WB_GRBLEVELS)) {
    TiffEntry *wb = mRootIFD->getEntryRecursive(FUJI_WB_GRBLEVELS);
    if (wb->count == 3) {
      mRaw->metadata.wbCoeffs[0] = wb->getFloat(1);
      mRaw->metadata.wbCoeffs[1] = wb->getFloat(0);
      mRaw->metadata.wbCoeffs[2] = wb->getFloat(2);
    }
  } else if (mRootIFD->hasEntryRecursive(FUJIOLDWB)) {
    TiffEntry *wb = mRootIFD->getEntryRecursive(FUJIOLDWB);
    if (wb->count == 8) {
      mRaw->metadata.wbCoeffs[0] = wb->getFloat(1);
      mRaw->metadata.wbCoeffs[1] = wb->getFloat(0);
      mRaw->metadata.wbCoeffs[2] = wb->getFloat(3);
    }
  }
}

int RafDecoder::isCompressed() {
  auto raw = mRootIFD->getIFDWithTag(FUJI_STRIPOFFSETS);
  uint32 height = 0;
  uint32 width = 0;

  if (raw->hasEntry(FUJI_RAWIMAGEFULLHEIGHT)) {
    height = raw->getEntry(FUJI_RAWIMAGEFULLHEIGHT)->getU32();
    width = raw->getEntry(FUJI_RAWIMAGEFULLWIDTH)->getU32();
  } else if (raw->hasEntry(IMAGEWIDTH)) {
    TiffEntry* e = raw->getEntry(IMAGEWIDTH);
    height = e->getU16(0);
    width = e->getU16(1);
  } else
    ThrowRDE("Unable to locate image size");

  if (width == 0 || height == 0 || width > 9216 || height > 6210)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  uint32 count = raw->getEntry(FUJI_STRIPBYTECOUNTS)->getU32();

  return count * 8 / (width * height) < 10;
}

} // namespace rawspeed
