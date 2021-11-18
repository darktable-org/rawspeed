/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real

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

#include "decoders/NakedDecoder.h"
#include "common/Common.h"                          // for BitOrder, BitOrd...
#include "common/Point.h"                           // for iPoint2D
#include "decoders/RawDecoderException.h"           // for ThrowRDE
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer, DataBuffer
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for Endianness, Endi...
#include "metadata/Camera.h"                        // for Camera, Hints
#include <map>                                      // for map
#include <stdexcept>                                // for out_of_range
#include <string>                                   // for string, basic_st...

using std::map;

namespace rawspeed {

class CameraMetaData;

NakedDecoder::NakedDecoder(const Buffer& file, const Camera* c)
    : RawDecoder(file), cam(c) {}

const map<std::string, BitOrder, std::less<>> NakedDecoder::order2enum = {
    {"plain", BitOrder::LSB},
    {"jpeg", BitOrder::MSB},
    {"jpeg16", BitOrder::MSB16},
    {"jpeg32", BitOrder::MSB32},
};

void NakedDecoder::parseHints() {
  const auto& cHints = cam->hints;
  const auto& make = cam->make.c_str();
  const auto& model = cam->model.c_str();

  auto parseHint = [&cHints, &make, &model](const std::string& name) {
    if (!cHints.has(name))
      ThrowRDE("%s %s: couldn't find %s", make, model, name.c_str());

    return cHints.get(name, 0U);
  };

  width = parseHint("full_width");
  height = parseHint("full_height");

  if (width == 0 || height == 0)
    ThrowRDE("%s %s: image is of zero size?", make, model);

  filesize = parseHint("filesize");
  offset = cHints.get("offset", 0);
  if (filesize == 0 || offset >= filesize)
    ThrowRDE("%s %s: no image data found", make, model);

  bits = cHints.get("bits", (filesize-offset)*8/width/height);
  if (bits == 0)
    ThrowRDE("%s %s: image bpp is invalid: %u", make, model, bits);

  auto order = cHints.get("order", std::string());
  if (!order.empty()) {
    try {
      bo = order2enum.at(order);
    } catch (const std::out_of_range&) {
      ThrowRDE("%s %s: unknown order: %s", make, model, order.c_str());
    }
  }
}

RawImage NakedDecoder::decodeRawInternal() {
  parseHints();

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  UncompressedDecompressor u(
      ByteStream(DataBuffer(mFile.getSubView(offset), Endianness::little)),
      mRaw);

  iPoint2D pos(0, 0);
  u.readUncompressedRaw(mRaw->dim, pos, width * bits / 8, bits, bo);

  return mRaw;
}

void NakedDecoder::checkSupportInternal(const CameraMetaData* meta) {
  this->checkCameraSupported(meta, cam->make, cam->model, cam->mode);
}

void NakedDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, cam->make, cam->model, cam->mode, 0);
}

} // namespace rawspeed
