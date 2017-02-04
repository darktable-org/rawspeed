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
#include "metadata/Camera.h"                        // for Camera
#include <cstdlib>                                  // for atoi
#include <map>                                      // for map, _Rb_tree_co...
#include <stdexcept>                                // for out_of_range
#include <string>                                   // for string, basic_st...
#include <utility>                                  // for pair

using namespace std;

namespace RawSpeed {

class CameraMetaData;

NakedDecoder::NakedDecoder(FileMap* file, Camera* c) :
    RawDecoder(file) {
  cam = c;
}

NakedDecoder::~NakedDecoder() = default;

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

  auto parseHint = [&cHints, &make, &model](const string& name, uint32& field,
                                            bool fatal = true) {
    const auto& hint = cHints.find(name);
    if (hint == cHints.end()) {
      if (fatal)
        ThrowRDE("Naked: %s %s: couldn't find %s", make, model, name.c_str());
      else
        return false;
    }

    const auto& tmp = hint->second;
    field = (uint32)atoi(tmp.c_str());

    return true;
  };

  parseHint("full_width", width);
  parseHint("full_height", height);
  parseHint("filesize", filesize);
  parseHint("offset", offset, false);

  if (!parseHint("bits", bits, false))
    bits = (filesize-offset)*8/width/height;

  if (cHints.find("order") != cHints.end()) {
    const auto& tmp = cHints.find("order")->second;

    try {
      bo = order2enum.at(tmp);
    } catch (std::out_of_range&) {
      ThrowRDE("Naked: %s %s: unknown order: %s", make, model, tmp.c_str());
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
