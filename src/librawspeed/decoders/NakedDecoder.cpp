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
#include "adt/Optional.h"
#include "adt/Point.h"
#include "bitstreams/BitStreams.h"
#include "common/RawImage.h"
#include "decoders/RawDecoder.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/UncompressedDecompressor.h"
#include "io/Buffer.h"
#include "io/ByteStream.h"
#include "io/Endianness.h"
#include "metadata/Camera.h"
#include <map>
#include <string>
#include <string_view>

using std::map;

namespace rawspeed {

class CameraMetaData;

NakedDecoder::NakedDecoder(Buffer file, const Camera* c)
    : RawDecoder(file), cam(c) {}

namespace {

Optional<BitOrder> getAsBitOrder(std::string_view s) {
  using enum BitOrder;
  if (s == "plain")
    return LSB;
  if (s == "jpeg")
    return MSB;
  if (s == "jpeg16")
    return MSB16;
  if (s == "jpeg32")
    return MSB32;
  return std::nullopt;
}

} // namespace

void NakedDecoder::parseHints() {
  const auto& cHints = cam->hints;
  const auto& make = cam->make.c_str();
  const auto& model = cam->model.c_str();

  auto parseHint = [&cHints, &make, &model](const std::string& name) {
    if (!cHints.contains(name))
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

  bits = cHints.get("bits", (filesize - offset) * 8 / width / height);
  if (bits == 0)
    ThrowRDE("%s %s: image bpp is invalid: %u", make, model, bits);

  auto order = cHints.get("order", std::string());
  if (!order.empty()) {
    auto bo_ = getAsBitOrder(order);
    if (!bo_)
      ThrowRDE("%s %s: unknown order: %s", make, model, order.c_str());
    bo = *bo_;
  }
}

RawImage NakedDecoder::decodeRawInternal() {
  parseHints();

  mRaw->dim = iPoint2D(width, height);

  iPoint2D pos(0, 0);
  UncompressedDecompressor u(
      ByteStream(DataBuffer(mFile.getSubView(offset), Endianness::little)),
      mRaw, iRectangle2D(pos, mRaw->dim), width * bits / 8, bits, bo);
  mRaw->createData();

  u.readUncompressedRaw();

  return mRaw;
}

void NakedDecoder::checkSupportInternal(const CameraMetaData* meta) {
  this->checkCameraSupported(meta, cam->make, cam->model, cam->mode);
}

void NakedDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, cam->make, cam->model, cam->mode, 0);
}

} // namespace rawspeed
