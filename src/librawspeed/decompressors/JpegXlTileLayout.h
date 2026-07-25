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

// The subset of a decoded JXL codestream's properties that has to agree with
// the DNG tags describing the tile. Mirrors the relevant JxlBasicInfo fields.
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
  BitDepthMismatch,
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
  case JpegXlStreamCheck::BitDepthMismatch:
    return "codestream bit depth does not match DNG BitsPerSample";
  }
  return "unknown";
}

// Decide whether a decoded codestream may be copied into the DNG tile.
//
// `cpp` is the raw image's samples-per-pixel and `dngBitsPerSample` the DNG's
// BitsPerSample (tag 0x0102) for this IFD.
//
// On the integer path the decoder asks libjxl for
// JXL_BIT_DEPTH_FROM_CODESTREAM, i.e. samples come out in the codestream's own
// range rather than stretched to fill uint16. That range is then interpreted by
// the rest of the pipeline against the DNG's BlackLevel/WhiteLevel, which are
// keyed to BitsPerSample. If the two depths disagree there is no way to know
// which one describes the intended scale, and either choice silently misexposes
// the image by a factor of (2^16 - 1)/(2^bps - 1). Refuse instead, naming both
// depths, so that a file hitting this is reported rather than quietly
// mis-decoded.
//
// Float tiles are decoded as JXL_TYPE_FLOAT, for which libjxl supports only
// JXL_BIT_DEPTH_FROM_PIXEL_FORMAT, and a DNG float BitsPerSample (16/24/32)
// does not describe the codestream's integer bitsPerSample. Nothing to compare,
// so the depth rule does not apply there.
[[nodiscard]] constexpr JpegXlStreamCheck
checkJpegXlStream(const JpegXlStreamProps& props, uint32_t cpp,
                  uint32_t dngBitsPerSample, bool isFloat) noexcept {
  if (props.width == 0 || props.height == 0)
    return JpegXlStreamCheck::EmptyImage;
  if (props.numColorChannels != cpp)
    return JpegXlStreamCheck::ColorChannelMismatch;
  // Requesting `cpp` channels would make libjxl drop any extra channel. Raw
  // mosaic/linear tiles have no meaning for one, so a codestream carrying one
  // is not a file we understand; do not silently discard part of it.
  if (props.numExtraChannels != 0)
    return JpegXlStreamCheck::ExtraChannelsUnsupported;
  if (!isFloat && props.bitsPerSample != dngBitsPerSample)
    return JpegXlStreamCheck::BitDepthMismatch;
  return JpegXlStreamCheck::Ok;
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
