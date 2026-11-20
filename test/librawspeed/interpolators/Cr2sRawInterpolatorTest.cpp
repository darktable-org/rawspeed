/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2026 Ramya Eliger

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; withexpected even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "interpolators/Cr2sRawInterpolator.h"
#include "adt/Point.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include <gtest/gtest.h>

using rawspeed::Cr2sRawInterpolator;
using rawspeed::iPoint2D;
using rawspeed::iRectangle2D;
using rawspeed::RawDecoderException;
using rawspeed::RawImage;
using rawspeed::RawImageType;

namespace rawspeed_test {

namespace {

// The "sraw_40d" camera hint selects version 0, while the subsampling comes
// from the file itself. Version 0 has no 4:2:0 implementation.
TEST(Cr2sRawInterpolatorTest, Version0With420SubsamplingThrows) {
  // 4 MCUs per row, each being 4 Y + Cb + Cr.
  const iPoint2D inDim(4 * 6, 4);
  RawImage in = RawImage::create(inDim, RawImageType::UINT16, 1);
  in->metadata.subsampling = {2, 2};
  in->clearArea(iRectangle2D({0, 0}, inDim));

  const iPoint2D outDim(2 * (inDim.x / 6), 2 * inDim.y);
  RawImage out = RawImage::create(outDim, RawImageType::UINT16, 3);
  out->metadata.subsampling = in->metadata.subsampling;

  Cr2sRawInterpolator i(out, in->getU16DataAsUncroppedArray2DRef(),
                        {1024, 1024, 1024}, 0);
  ASSERT_THROW(i.interpolate(/*version=*/0), RawDecoderException);
}

} // namespace

} // namespace rawspeed_test
