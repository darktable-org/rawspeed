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
using rawspeed::JpegXlBitDepthMode;
using rawspeed::jpegXlBitDepthMode;
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
// checkJpegXlStream: what actually disqualifies a codestream
//

TEST(JpegXlStreamCheckTest, S23LinearRawIsAccepted) {
  EXPECT_EQ(checkJpegXlStream(s23LinearRaw(), /*cpp=*/3,
                              /*whiteLevel=*/65535, /*isFloat=*/false),
            JpegXlStreamCheck::Ok);
}

// A single-channel CFA mosaic tile whose WhiteLevel sits inside its own depth.
TEST(JpegXlStreamCheckTest, MonochromeCfaTileIsAccepted) {
  const JpegXlStreamProps props = {/*width=*/256, /*height=*/256,
                                   /*bitsPerSample=*/14,
                                   /*numColorChannels=*/1,
                                   /*numExtraChannels=*/0};
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/1, /*whiteLevel=*/16383,
                              /*isFloat=*/false),
            JpegXlStreamCheck::Ok);
}

// The regression this file exists for, measured on a real file:
// `iPhone 17 Pro RAW IMG_0031.DNG` declares BitsPerSample = 10, yet jxlinfo
// reports its tiles as 16-bit RGB 2016x2016, with WhiteLevel = 65535. An
// earlier revision compared the codestream depth against BitsPerSample and
// refused the file outright. BitsPerSample is not the range indicator, so this
// must decode.
TEST(JpegXlStreamCheckTest, AppleProRawSixteenBitCodestreamInTenBitDngIsOk) {
  const JpegXlStreamProps props = {/*width=*/2016, /*height=*/2016,
                                   /*bitsPerSample=*/16,
                                   /*numColorChannels=*/3,
                                   /*numExtraChannels=*/0};
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*whiteLevel=*/65535,
                              /*isFloat=*/false),
            JpegXlStreamCheck::Ok);
}

// The mirror image of the Apple case: Panasonic declares BitsPerSample = 16 but
// embeds a 12-bit codestream, with WhiteLevel = 63232 = 3952 * 16, i.e. keyed
// to the stretched range. Also must not be refused for the depth disagreement.
TEST(JpegXlStreamCheckTest, PanasonicTwelveBitCodestreamIsOk) {
  JpegXlStreamProps props = s23LinearRaw();
  props.bitsPerSample = 12;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*whiteLevel=*/63232,
                              /*isFloat=*/false),
            JpegXlStreamCheck::Ok);
}

// A depth the uint16 output path cannot represent is a different matter: there
// is no scaling that recovers it, so it is refused.
TEST(JpegXlStreamCheckTest, DepthDeeperThanUint16IsRejected) {
  JpegXlStreamProps props = s23LinearRaw();
  props.bitsPerSample = 17;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*whiteLevel=*/65535,
                              /*isFloat=*/false),
            JpegXlStreamCheck::BitDepthUnsupported);

  props.bitsPerSample = 32;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*whiteLevel=*/65535,
                              /*isFloat=*/false),
            JpegXlStreamCheck::BitDepthUnsupported);
}

TEST(JpegXlStreamCheckTest, ZeroDepthIsRejected) {
  JpegXlStreamProps props = s23LinearRaw();
  props.bitsPerSample = 0;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*whiteLevel=*/65535,
                              /*isFloat=*/false),
            JpegXlStreamCheck::BitDepthUnsupported);
}

// Float tiles decode as JXL_TYPE_FLOAT, where bitsPerSample is paired with an
// exponent field, so neither integer bound applies.
TEST(JpegXlStreamCheckTest, FloatPathIgnoresDepthBounds) {
  JpegXlStreamProps props = s23LinearRaw();
  props.bitsPerSample = 32;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*whiteLevel=*/1,
                              /*isFloat=*/true),
            JpegXlStreamCheck::Ok);
}

// No uint16 output can reach a WhiteLevel above 65535.
TEST(JpegXlStreamCheckTest, WhiteLevelAboveUint16IsRejected) {
  EXPECT_EQ(checkJpegXlStream(s23LinearRaw(), /*cpp=*/3, /*whiteLevel=*/65536,
                              /*isFloat=*/false),
            JpegXlStreamCheck::WhiteLevelUnrepresentable);
}

// An unknown WhiteLevel is not an error; it just means the default scaling.
TEST(JpegXlStreamCheckTest, UnknownWhiteLevelIsAccepted) {
  EXPECT_EQ(checkJpegXlStream(s23LinearRaw(), /*cpp=*/3, /*whiteLevel=*/0,
                              /*isFloat=*/false),
            JpegXlStreamCheck::Ok);
}

TEST(JpegXlStreamCheckTest, ColorChannelCountMustMatchCpp) {
  const JpegXlStreamProps props = s23LinearRaw(); // 3 color channels
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/1, /*whiteLevel=*/65535,
                              /*isFloat=*/false),
            JpegXlStreamCheck::ColorChannelMismatch);
}

// Requesting cpp channels would make libjxl silently drop the extra one.
TEST(JpegXlStreamCheckTest, ExtraChannelsAreRejectedNotDropped) {
  JpegXlStreamProps props = s23LinearRaw();
  props.numExtraChannels = 1;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*whiteLevel=*/65535,
                              /*isFloat=*/false),
            JpegXlStreamCheck::ExtraChannelsUnsupported);
}

TEST(JpegXlStreamCheckTest, ZeroSizedCodestreamIsRejected) {
  JpegXlStreamProps props = s23LinearRaw();
  props.width = 0;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*whiteLevel=*/65535,
                              /*isFloat=*/false),
            JpegXlStreamCheck::EmptyImage);

  props = s23LinearRaw();
  props.height = 0;
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*whiteLevel=*/65535,
                              /*isFloat=*/false),
            JpegXlStreamCheck::EmptyImage);
}

// The checks are ordered so that the most structural problem is reported first.
TEST(JpegXlStreamCheckTest, EmptinessOutranksOtherFaults) {
  const JpegXlStreamProps props = {/*width=*/0, /*height=*/0,
                                   /*bitsPerSample=*/32,
                                   /*numColorChannels=*/4,
                                   /*numExtraChannels=*/2};
  EXPECT_EQ(checkJpegXlStream(props, /*cpp=*/3, /*whiteLevel=*/70000,
                              /*isFloat=*/false),
            JpegXlStreamCheck::EmptyImage);
}

//
// jpegXlBitDepthMode: which range the decoded samples must land on
//

// Apple: WhiteLevel 65535 fits a 16-bit codestream exactly, so take it as-is.
// (For a 16-bit codestream the two modes coincide anyway.)
TEST(JpegXlBitDepthModeTest, AppleFullRangeUsesCodestreamRange) {
  JpegXlStreamProps props = s23LinearRaw();
  props.bitsPerSample = 16;
  EXPECT_EQ(jpegXlBitDepthMode(props, /*whiteLevel=*/65535, /*isFloat=*/false),
            JpegXlBitDepthMode::FromCodestream);
}

// Panasonic: WhiteLevel 63232 cannot be expressed in 12 bits (max 4095), so it
// must be describing the stretched range. Keep libjxl's rescale, which lands
// 3952 on 63232.
TEST(JpegXlBitDepthModeTest, WhiteLevelBeyondCodestreamRangeStretches) {
  JpegXlStreamProps props = s23LinearRaw();
  props.bitsPerSample = 12;
  EXPECT_EQ(jpegXlBitDepthMode(props, /*whiteLevel=*/63232, /*isFloat=*/false),
            JpegXlBitDepthMode::PixelFormatDefault);
}

// A 12-bit codestream whose WhiteLevel is also 12-bit is already on the right
// scale; stretching it would make it ~16x too bright.
TEST(JpegXlBitDepthModeTest, MatchingTwelveBitUsesCodestreamRange) {
  JpegXlStreamProps props = s23LinearRaw();
  props.bitsPerSample = 12;
  EXPECT_EQ(jpegXlBitDepthMode(props, /*whiteLevel=*/4095, /*isFloat=*/false),
            JpegXlBitDepthMode::FromCodestream);
}

// Adobe's 03_jxl_bayer_raw_integer.dng: 16-bit codestream, WhiteLevel 16383.
// WhiteLevel below the codestream max is normal (data simply does not reach
// full scale) and must not trigger a rescale.
TEST(JpegXlBitDepthModeTest, WhiteLevelBelowCodestreamMaxIsNotRescaled) {
  JpegXlStreamProps props = s23LinearRaw();
  props.bitsPerSample = 16;
  props.numColorChannels = 1;
  EXPECT_EQ(jpegXlBitDepthMode(props, /*whiteLevel=*/16383, /*isFloat=*/false),
            JpegXlBitDepthMode::FromCodestream);
}

// Nothing to key the decision to; keep the libjxl default.
TEST(JpegXlBitDepthModeTest, UnknownWhiteLevelKeepsDefault) {
  EXPECT_EQ(jpegXlBitDepthMode(s23LinearRaw(), /*whiteLevel=*/0,
                               /*isFloat=*/false),
            JpegXlBitDepthMode::PixelFormatDefault);
}

// libjxl supports only the default for float output.
TEST(JpegXlBitDepthModeTest, FloatKeepsDefault) {
  EXPECT_EQ(jpegXlBitDepthMode(s23LinearRaw(), /*whiteLevel=*/1,
                               /*isFloat=*/true),
            JpegXlBitDepthMode::PixelFormatDefault);
}

// An out-of-range depth never reaches the scaling decision (checkJpegXlStream
// refuses it first), but the mode must still be well-defined for it.
TEST(JpegXlBitDepthModeTest, OutOfRangeDepthKeepsDefault) {
  JpegXlStreamProps props = s23LinearRaw();
  props.bitsPerSample = 17;
  EXPECT_EQ(jpegXlBitDepthMode(props, /*whiteLevel=*/65535, /*isFloat=*/false),
            JpegXlBitDepthMode::PixelFormatDefault);

  props.bitsPerSample = 0;
  EXPECT_EQ(jpegXlBitDepthMode(props, /*whiteLevel=*/65535, /*isFloat=*/false),
            JpegXlBitDepthMode::PixelFormatDefault);
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
