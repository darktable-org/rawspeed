/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2026 Mayk Thewessen

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

#include "decompressors/JpegXlTileLayout.h"
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>

using rawspeed::checkJpegXlStream;
using rawspeed::clipJpegXlTile;
using rawspeed::JpegXlStreamCheck;
using rawspeed::JpegXlStreamProps;
using rawspeed::jpegXlTileBufferBytes;
using rawspeed::JpegXlTileExtent;

namespace rawspeed_test {

namespace {

// The Samsung Galaxy S23 Expert RAW layout reported on rawspeed#971:
// 4000x3000, 16-bit, three-channel LinearRaw.
constexpr JpegXlStreamProps s23LinearRaw() {
  return {/*width=*/4000, /*height=*/3000, /*bitsPerSample=*/16,
          /*numColorChannels=*/3, /*numExtraChannels=*/0};
}

} // namespace

//
// checkJpegXlStream: codestream vs DNG agreement
//

TEST(JpegXlStreamCheckTest, S23LinearRawIsAccepted) {
  EXPECT_EQ(checkJpegXlStream(s23LinearRaw(), /*cpp=*/3,
                              /*dngBitsPerSample=*/16, /*isFloat=*/false),
            JpegXlStreamCheck::Ok);
}

// A single-channel CFA mosaic tile at the same depth.
TEST(JpegXlStreamCheckTest, MonochromeCfaTileIsAccepted) {
  const JpegXlStreamProps props = {/*width=*/256, /*height=*/256,
                                   /*bitsPerSample=*/14,
                                   /*numColorChannels=*/1,
                                   /*numExtraChannels=*/0};
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/1, /*dngBitsPerSample=*/14,
                              /*isFloat=*/false),
            JpegXlStreamCheck::Ok);
}

// The regression this file exists for. libjxl's default output bit depth
// (JXL_BIT_DEPTH_FROM_PIXEL_FORMAT) stretches a sub-16-bit codestream to fill
// uint16, which misexposes the tile by (2^16-1)/(2^bps-1) against the DNG's
// WhiteLevel. The decoder now pins the output to the codestream range instead,
// so a depth disagreement is unresolvable and must be refused rather than
// decoded into a plausible-looking but wrongly scaled image.
TEST(JpegXlStreamCheckTest, TwelveBitCodestreamInSixteenBitDngIsRejected) {
  JpegXlStreamProps props = s23LinearRaw();
  props.bitsPerSample = 12;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*dngBitsPerSample=*/16,
                              /*isFloat=*/false),
            JpegXlStreamCheck::BitDepthMismatch);
}

TEST(JpegXlStreamCheckTest, FourteenBitCodestreamInSixteenBitDngIsRejected) {
  JpegXlStreamProps props = s23LinearRaw();
  props.bitsPerSample = 14;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*dngBitsPerSample=*/16,
                              /*isFloat=*/false),
            JpegXlStreamCheck::BitDepthMismatch);
}

// Matching sub-16-bit depths are fine: the codestream range is exactly what the
// DNG's levels describe.
TEST(JpegXlStreamCheckTest, MatchingTwelveBitDepthIsAccepted) {
  JpegXlStreamProps props = s23LinearRaw();
  props.bitsPerSample = 12;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*dngBitsPerSample=*/12,
                              /*isFloat=*/false),
            JpegXlStreamCheck::Ok);
}

// Float tiles decode as JXL_TYPE_FLOAT, where libjxl only supports the default
// bit depth handling and the DNG's float BitsPerSample does not describe the
// codestream's integer bitsPerSample. The depth rule must not fire there.
TEST(JpegXlStreamCheckTest, FloatPathIgnoresBitDepthDisagreement) {
  JpegXlStreamProps props = s23LinearRaw();
  props.bitsPerSample = 32;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*dngBitsPerSample=*/16,
                              /*isFloat=*/true),
            JpegXlStreamCheck::Ok);
}

TEST(JpegXlStreamCheckTest, ColorChannelCountMustMatchCpp) {
  const JpegXlStreamProps props = s23LinearRaw(); // 3 color channels
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/1, /*dngBitsPerSample=*/16,
                              /*isFloat=*/false),
            JpegXlStreamCheck::ColorChannelMismatch);
}

// Requesting cpp channels would make libjxl silently drop the extra one.
TEST(JpegXlStreamCheckTest, ExtraChannelsAreRejectedNotDropped) {
  JpegXlStreamProps props = s23LinearRaw();
  props.numExtraChannels = 1;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*dngBitsPerSample=*/16,
                              /*isFloat=*/false),
            JpegXlStreamCheck::ExtraChannelsUnsupported);
}

TEST(JpegXlStreamCheckTest, ZeroSizedCodestreamIsRejected) {
  JpegXlStreamProps props = s23LinearRaw();
  props.width = 0;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*dngBitsPerSample=*/16,
                              /*isFloat=*/false),
            JpegXlStreamCheck::EmptyImage);

  props = s23LinearRaw();
  props.height = 0;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*dngBitsPerSample=*/16,
                              /*isFloat=*/false),
            JpegXlStreamCheck::EmptyImage);
}

// The checks are ordered so that the most structural problem is reported first.
TEST(JpegXlStreamCheckTest, EmptinessOutranksOtherFaults) {
  const JpegXlStreamProps props = {/*width=*/0, /*height=*/0,
                                   /*bitsPerSample=*/12,
                                   /*numColorChannels=*/4,
                                   /*numExtraChannels=*/2};
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*dngBitsPerSample=*/16,
                              /*isFloat=*/false),
            JpegXlStreamCheck::EmptyImage);
}

//
// clipJpegXlTile: tile placement geometry
//

TEST(JpegXlTileExtentTest, InteriorTileIsNotClipped) {
  const JpegXlTileExtent e = clipJpegXlTile(/*imgW=*/4000, /*imgH=*/3000,
                                            /*offX=*/512, /*offY=*/256,
                                            /*jxlW=*/256, /*jxlH=*/256);
  EXPECT_EQ(e.w, 256U);
  EXPECT_EQ(e.h, 256U);
  EXPECT_FALSE(e.empty());
}

// 4000 is not a multiple of 256, so the last column of tiles overhangs.
TEST(JpegXlTileExtentTest, RightEdgeTileIsTruncated) {
  const JpegXlTileExtent e = clipJpegXlTile(/*imgW=*/4000, /*imgH=*/3000,
                                            /*offX=*/3840, /*offY=*/0,
                                            /*jxlW=*/256, /*jxlH=*/256);
  EXPECT_EQ(e.w, 160U); // 4000 - 3840
  EXPECT_EQ(e.h, 256U);
}

TEST(JpegXlTileExtentTest, BottomEdgeTileIsTruncated) {
  const JpegXlTileExtent e = clipJpegXlTile(/*imgW=*/4000, /*imgH=*/3000,
                                            /*offX=*/0, /*offY=*/2816,
                                            /*jxlW=*/256, /*jxlH=*/256);
  EXPECT_EQ(e.w, 256U);
  EXPECT_EQ(e.h, 184U); // 3000 - 2816
}

TEST(JpegXlTileExtentTest, CornerTileIsTruncatedInBothAxes) {
  const JpegXlTileExtent e = clipJpegXlTile(/*imgW=*/4000, /*imgH=*/3000,
                                            /*offX=*/3840, /*offY=*/2816,
                                            /*jxlW=*/256, /*jxlH=*/256);
  EXPECT_EQ(e.w, 160U);
  EXPECT_EQ(e.h, 184U);
}

// A single-tile image whose codestream exactly fills it.
TEST(JpegXlTileExtentTest, SingleTileCoveringWholeImage) {
  const JpegXlTileExtent e = clipJpegXlTile(/*imgW=*/4000, /*imgH=*/3000,
                                            /*offX=*/0, /*offY=*/0,
                                            /*jxlW=*/4000, /*jxlH=*/3000);
  EXPECT_EQ(e.w, 4000U);
  EXPECT_EQ(e.h, 3000U);
}

// The regression: `imgW - offX` on uint32_t wraps to ~4e9 when offX >= imgW,
// which would make the copy loop run far past the end of the raw buffer.
TEST(JpegXlTileExtentTest, OffsetAtImageEdgeDoesNotUnderflow) {
  const JpegXlTileExtent e = clipJpegXlTile(/*imgW=*/4000, /*imgH=*/3000,
                                            /*offX=*/4000, /*offY=*/0,
                                            /*jxlW=*/256, /*jxlH=*/256);
  EXPECT_TRUE(e.empty());
  EXPECT_EQ(e.w, 0U);
}

TEST(JpegXlTileExtentTest, OffsetPastImageDoesNotUnderflow) {
  const JpegXlTileExtent x = clipJpegXlTile(/*imgW=*/4000, /*imgH=*/3000,
                                            /*offX=*/9999, /*offY=*/0,
                                            /*jxlW=*/256, /*jxlH=*/256);
  EXPECT_TRUE(x.empty());

  const JpegXlTileExtent y = clipJpegXlTile(/*imgW=*/4000, /*imgH=*/3000,
                                            /*offX=*/0, /*offY=*/9999,
                                            /*jxlW=*/256, /*jxlH=*/256);
  EXPECT_TRUE(y.empty());
}

//
// jpegXlTileBufferBytes: required output buffer size
//

TEST(JpegXlTileBufferTest, InterleavedTileSize) {
  // 256x256, 3 channels, uint16.
  EXPECT_EQ(jpegXlTileBufferBytes(256, 256, 3, sizeof(uint16_t)),
            size_t{256} * 256 * 3 * 2);
  // Single-channel float.
  EXPECT_EQ(jpegXlTileBufferBytes(256, 256, 1, sizeof(float)),
            size_t{256} * 256 * 4);
}

TEST(JpegXlTileBufferTest, S23FullFrameSize) {
  EXPECT_EQ(jpegXlTileBufferBytes(4000, 3000, 3, sizeof(uint16_t)),
            size_t{72'000'000});
}

// The product must be computed in size_t: 65535*65535*3*2 overflows uint32_t.
TEST(JpegXlTileBufferTest, LargeTileDoesNotOverflow) {
  constexpr size_t expected = size_t{65535} * 65535 * 3 * 2;
  static_assert(expected > size_t{UINT32_MAX},
                "test is meaningless if it fits in 32 bits");
  EXPECT_EQ(jpegXlTileBufferBytes(65535, 65535, 3, sizeof(uint16_t)), expected);
}

} // namespace rawspeed_test
