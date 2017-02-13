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
#include "metadata/Camera.h"                        // for Camera, Hints
#include <map>                                      // for map
#include <stdexcept>                                // for out_of_range
#include <string>                                   // for string, basic_st...

using namespace std;

namespace RawSpeed {

class CameraMetaData;

NakedDecoder::NakedDecoder(FileMap* file, Camera* c) :
    RawDecoder(file) {
  cam = c;
}

static const map<string, BitOrder> order2enum = {
    {"plain", BitOrder_Plain},
    {"jpeg", BitOrder_Jpeg},
    {"jpeg16", BitOrder_Jpeg16},
    {"jpeg32", BitOrder_Jpeg32},
};

void NakedDecoder::parseHints() {
  const auto& cHints = cam->hints;
  const auto& make = cam->make.c_str();
  const auto& model = cam->model.c_str();

  auto parseHint = [&cHints, &make, &model](const string& name) -> uint32 {
    if (!cHints.has(name))
      ThrowRDE("Naked: %s %s: couldn't find %s", make, model, name.c_str());

    return cHints.get(name, 0u);
  };

  width = parseHint("full_width");
  height = parseHint("full_height");
  filesize = parseHint("filesize");
  offset = cHints.get("offset", 0);
  bits = cHints.get("bits", (filesize-offset)*8/width/height);

  auto order = cHints.get("order", string());
  if (!order.empty()) {
    try {
      bo = order2enum.at(order);
    } catch (std::out_of_range&) {
      ThrowRDE("Naked: %s %s: unknown order: %s", make, model, order.c_str());
    }
  }
}

RawImage NakedDecoder::decodeRawInternal() {
  parseHints();

  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  UncompressedDecompressor u(*mFile, offset, mRaw, uncorrectedRawValues);

  iPoint2D pos(0, 0);
  u.readUncompressedRaw(mRaw->dim, pos, width * bits / 8, bits, bo);

  return mRaw;
}

void NakedDecoder::checkSupportInternal(CameraMetaData *meta) {
  this->checkCameraSupported(meta, cam->make, cam->model, cam->mode);
}

void NakedDecoder::decodeMetaDataInternal(CameraMetaData *meta) {
  setMetaData(meta, cam->make, cam->model, cam->mode, 0);
}

} // namespace RawSpeed
