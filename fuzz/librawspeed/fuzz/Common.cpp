/*
    RawSpeed - RAW file decoder.

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

#include "fuzz/Common.h"
#include "common/Point.h"    // for iPoint2D, iPoint2D::value_type
#include "common/RawImage.h" // for RawImage, RawImageData, RawImageType
#include "io/ByteStream.h"   // for ByteStream
#include "io/IOException.h"  // for ThrowException, ThrowRSE, ThrowIOE
#include <cstdint>           // for uint32_t, int32_t

rawspeed::RawImage CreateRawImage(rawspeed::ByteStream& bs) {
  const uint32_t width = bs.getU32();
  const uint32_t height = bs.getU32();
  const uint32_t type = bs.getU32();
  const uint32_t cpp = bs.getU32();
  const uint32_t isCFA = bs.getU32();

  if (type != static_cast<uint32_t>(rawspeed::RawImageType::UINT16) &&
      type != static_cast<uint32_t>(rawspeed::RawImageType::F32))
    ThrowRSE("Unknown image type: %u", type);

  rawspeed::RawImage mRaw(
      rawspeed::RawImage::create(static_cast<rawspeed::RawImageType>(type)));

  mRaw->dim =
      rawspeed::iPoint2D(static_cast<rawspeed::iPoint2D::value_type>(width),
                         static_cast<rawspeed::iPoint2D::value_type>(height));
  mRaw->setCpp(cpp);
  mRaw->isCFA = isCFA;

  return mRaw;
}

rawspeed::ColorFilterArray CreateCFA(rawspeed::ByteStream& bs) {
  const int32_t cfaWidth = bs.getI32();
  const int32_t cfaHeight = bs.getI32();

  rawspeed::iPoint2D cfaSize(cfaWidth, cfaHeight);
  if (!cfaSize.hasPositiveArea())
    ThrowIOE("Bad CFA size.");

  rawspeed::ColorFilterArray cfa;
  cfa.setSize(cfaSize);
  (void)bs.check(cfaSize.area(), 4);

  for (auto x = 0; x < cfaWidth; x++) {
    for (auto y = 0; y < cfaHeight; y++) {
      const uint32_t color = bs.getU32();
      if (color >= static_cast<uint32_t>(rawspeed::CFAColor::END))
        ThrowRSE("Unknown color: %u", color);

      cfa.setColorAt(rawspeed::iPoint2D(x, y),
                     static_cast<rawspeed::CFAColor>(color));
    }
  }

  return cfa;
}
