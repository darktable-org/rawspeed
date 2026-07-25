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

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace rawspeed {

// Geometry and validation helpers for the DNG 1.7 JPEG XL tile path (TIFF
// Compression 52546), factored out of JpegXlDecompressor.
//
// These are deliberately pure and free of any <jxl/*.h> dependency, so that the
// index arithmetic and the codestream-vs-DNG agreement rules can be unit-tested
// on every build, with or without libjxl, and without needing a sample file.

// The subset of a decoded JXL codestream's properties the tile path has to
// reason about. Mirrors the relevant JxlBasicInfo fields.
struct JpegXlStreamProps final {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t bitsPerSample = 0;
  uint32_t numColorChannels = 0;
  uint32_t numExtraChannels = 0;
};

enum class JpegXlStreamCheck {
  Ok,
  EmptyImage,
  ColorChannelMismatch,
  ExtraChannelsUnsupported,
  BitDepthUnsupported,
  WhiteLevelUnrepresentable,
};

[[nodiscard]] constexpr const char* toString(JpegXlStreamCheck check) noexcept {
  switch (check) {
  case JpegXlStreamCheck::Ok:
    return "ok";
  case JpegXlStreamCheck::EmptyImage:
    return "codestream has zero width or height";
  case JpegXlStreamCheck::ColorChannelMismatch:
    return "codestream color channel count does not match DNG SamplesPerPixel";
  case JpegXlStreamCheck::ExtraChannelsUnsupported:
    return "codestream has extra (non-color) channels";
  case JpegXlStreamCheck::BitDepthUnsupported:
    return "codestream bit depth does not fit the uint16 output";
  case JpegXlStreamCheck::WhiteLevelUnrepresentable:
    return "DNG WhiteLevel exceeds the uint16 output range";
  }
  return "unknown";
}

// Largest sample value an integer codestream of `bitsPerSample` bits can hold.
// Only meaningful for 1..16; callers reject anything else first.
[[nodiscard]] constexpr uint32_t
jpegXlCodestreamMax(uint32_t bitsPerSample) noexcept {
  return (uint32_t{1} << bitsPerSample) - 1;
}

// Widest value the integer output path (JXL_TYPE_UINT16) can carry.
constexpr uint32_t JpegXlMaxOutputValue = 65535;

// Decide whether a decoded codestream may be copied into the DNG tile.
//
// `cpp` is the raw image's samples-per-pixel and `whiteLevel` the DNG's
// WhiteLevel (tag 0xC61D, or the BitsPerSample-derived default) for this IFD.
// Pass 0 when it is unknown.
//
// Deliberately NOT checked: the codestream's bitsPerSample against the DNG's
// BitsPerSample. Those two legitimately disagree in the wild, because
// BitsPerSample is not what the sample values are scaled to:
//   - Apple ProRAW (iPhone 17 Pro) declares BitsPerSample = 10 while embedding
//     a 16-bit codestream, with WhiteLevel = 65535.
//   - Panasonic declares BitsPerSample = 16 while embedding a 12-bit
//     codestream, with WhiteLevel = 63232 = 3952 * 16.
// Rejecting on disagreement would refuse both of those real files. WhiteLevel
// is the tag that describes the intended range, so it is what the scaling
// decision is keyed to (see jpegXlBitDepthMode) and a depth disagreement is not
// by itself an error. What IS worth refusing is a depth the output path cannot
// represent at all, and a WhiteLevel no uint16 scaling could reach.
[[nodiscard]] constexpr JpegXlStreamCheck
checkJpegXlStream(const JpegXlStreamProps& props, uint32_t cpp,
                  uint32_t whiteLevel, bool isFloat) noexcept {
  if (props.width == 0 || props.height == 0)
    return JpegXlStreamCheck::EmptyImage;
  if (props.numColorChannels != cpp)
    return JpegXlStreamCheck::ColorChannelMismatch;
  // Requesting `cpp` channels would make libjxl drop any extra channel. Raw
  // mosaic/linear tiles have no meaning for one, so a codestream carrying one
  // is not a file we understand; do not silently discard part of it.
  if (props.numExtraChannels != 0)
    return JpegXlStreamCheck::ExtraChannelsUnsupported;
  // The integer path decodes as JXL_TYPE_UINT16, so a deeper codestream has no
  // representation, and 0 is not a valid depth. Float tiles decode as
  // JXL_TYPE_FLOAT, where bitsPerSample is paired with an exponent field and
  // neither bound applies.
  if (!isFloat && (props.bitsPerSample == 0 || props.bitsPerSample > 16))
    return JpegXlStreamCheck::BitDepthUnsupported;
  if (!isFloat && whiteLevel > JpegXlMaxOutputValue)
    return JpegXlStreamCheck::WhiteLevelUnrepresentable;
  return JpegXlStreamCheck::Ok;
}

enum class JpegXlBitDepthMode {
  // libjxl's default (JXL_BIT_DEPTH_FROM_PIXEL_FORMAT): the codestream is
  // rescaled to fill the pixel format's range, so a b-bit codestream is
  // stretched to fill uint16.
  PixelFormatDefault,
  // JXL_BIT_DEPTH_FROM_CODESTREAM: samples come out in the codestream's own
  // range, unscaled.
  FromCodestream,
};

// Pick the libjxl output bit depth so decoded values land on the scale the
// DNG's WhiteLevel describes.
//
// If WhiteLevel fits inside the codestream's own range, then that range is the
// scale the DNG's levels are keyed to and the samples must not be touched. If
// WhiteLevel is larger than the codestream can express, it can only be
// describing the stretched full-uint16 range, and libjxl's default rescale is
// exactly what makes the two agree (Panasonic: 12-bit codestream, WhiteLevel
// 63232 = 3952 * 16). For a 16-bit codestream the two modes coincide.
//
// An unknown WhiteLevel (0) keeps libjxl's default, which is the behaviour that
// predates this decision.
[[nodiscard]] constexpr JpegXlBitDepthMode
jpegXlBitDepthMode(const JpegXlStreamProps& props, uint32_t whiteLevel,
                   bool isFloat) noexcept {
  // Float output supports nothing but the default.
  if (isFloat || whiteLevel == 0 || props.bitsPerSample == 0 ||
      props.bitsPerSample > 16)
    return JpegXlBitDepthMode::PixelFormatDefault;
  if (whiteLevel <= jpegXlCodestreamMax(props.bitsPerSample))
    return JpegXlBitDepthMode::FromCodestream;
  return JpegXlBitDepthMode::PixelFormatDefault;
}

struct JpegXlTileExtent final {
  uint32_t w = 0;
  uint32_t h = 0;

  [[nodiscard]] constexpr bool empty() const noexcept {
    return w == 0 || h == 0;
  }
};

// Clip a decoded tile placed at (offX, offY) against the raw image bounds.
//
// Tiles on the right/bottom edge are legitimately larger than the image area
// left to fill, so the copy is truncated to fit. An offset at or beyond the
// image bounds is rejected explicitly rather than left to unsigned subtraction,
// which would wrap to ~4e9 and turn a bad tile offset into a wild write.
[[nodiscard]] constexpr JpegXlTileExtent
clipJpegXlTile(uint32_t imgW, uint32_t imgH, uint32_t offX, uint32_t offY,
               uint32_t jxlW, uint32_t jxlH) noexcept {
  if (offX >= imgW || offY >= imgH)
    return {};
  return {std::min(imgW - offX, jxlW), std::min(imgH - offY, jxlH)};
}

// Bytes libjxl must hand back for a fully interleaved jxlW x jxlH tile of `cpp`
// samples of `sampleSize` bytes each (JxlPixelFormat align = 0, so rows are
// tightly packed).
//
// Widened to size_t before multiplying: the product of plausible tile
// dimensions overflows 32 bits well before it overflows the buffer.
[[nodiscard]] constexpr size_t
jpegXlTileBufferBytes(uint32_t jxlW, uint32_t jxlH, uint32_t cpp,
                      size_t sampleSize) noexcept {
  return static_cast<size_t>(jxlW) * static_cast<size_t>(jxlH) *
         static_cast<size_t>(cpp) * sampleSize;
}

} // namespace rawspeed
