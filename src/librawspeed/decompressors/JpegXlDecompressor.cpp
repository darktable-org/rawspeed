/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2026 darktable developers

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

#include "rawspeedconfig.h" // IWYU pragma: keep

#ifdef HAVE_JPEGXL

#include "adt/Array2DRef.h"
#include "adt/Point.h"
#include "common/RawImage.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/JpegXlDecompressor.h"
#include "decompressors/JpegXlTileLayout.h"
#include <cstddef>
#include <cstdint>
#include <jxl/decode.h>
#include <jxl/types.h>
#include <vector>

namespace rawspeed {

namespace {
// RAII wrapper so JxlDecoderDestroy runs on every exit path (incl. throws).
struct JxlDecoderGuard final {
  JxlDecoder* dec;
  explicit JxlDecoderGuard(JxlDecoder* d) : dec(d) {}
  JxlDecoderGuard(const JxlDecoderGuard&) = delete;
  JxlDecoderGuard(JxlDecoderGuard&&) = delete;
  JxlDecoderGuard& operator=(const JxlDecoderGuard&) = delete;
  JxlDecoderGuard& operator=(JxlDecoderGuard&&) = delete;
  ~JxlDecoderGuard() { JxlDecoderDestroy(dec); }
};

// Copy the decoded, interleaved JXL tile into the raw buffer at (offX,offY).
// Templated on the sample type so the integer (uint16_t) and float (float)
// output paths share one implementation.
template <typename T>
void copyTile(const Array2DRef<T> out, const T* pixels, uint32_t jxl_w,
              uint32_t copy_w, uint32_t copy_h, uint32_t cpp, uint32_t offX,
              uint32_t offY) {
  for (uint32_t row = 0; row < copy_h; ++row) {
    for (uint32_t col = 0; col < cpp * copy_w; ++col) {
      out(static_cast<int>(row + offY), static_cast<int>(cpp * offX + col)) =
          pixels[(static_cast<size_t>(row) * jxl_w * cpp) + col];
    }
  }
}
} // namespace

void JpegXlDecompressor::decode(uint32_t offX, uint32_t offY) {
  JxlDecoder* dec = JxlDecoderCreate(nullptr);
  if (dec == nullptr)
    ThrowRDE("JXL: JxlDecoderCreate failed");
  JxlDecoderGuard guard(dec);

  if (JXL_DEC_SUCCESS !=
      JxlDecoderSubscribeEvents(dec, JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE))
    ThrowRDE("JXL: JxlDecoderSubscribeEvents failed");

  // rawspeed/darktable handle orientation themselves; do not auto-rotate.
  if (JXL_DEC_SUCCESS != JxlDecoderSetKeepOrientation(dec, JXL_TRUE))
    ThrowRDE("JXL: JxlDecoderSetKeepOrientation failed");

  if (JXL_DEC_SUCCESS !=
      JxlDecoderSetInput(dec, input.begin(), input.getSize()))
    ThrowRDE("JXL: JxlDecoderSetInput failed");
  JxlDecoderCloseInput(dec);

  const uint32_t cpp = mRaw->getCpp();
  // Float DNGs (e.g. linear raw float) store an F32 buffer; integer DNGs store
  // uint16. Decode JXL directly into whichever sample type mRaw expects, so the
  // tile lands in the matching typed view below.
  const bool isFloat = mRaw->getDataType() == RawImageType::F32;
  const size_t sampleSize = isFloat ? sizeof(float) : sizeof(uint16_t);
  const JxlPixelFormat fmt = {
      /*num_channels=*/cpp,
      /*data_type=*/isFloat ? JXL_TYPE_FLOAT : JXL_TYPE_UINT16,
      /*endianness=*/JXL_LITTLE_ENDIAN,
      /*align=*/0};

  JpegXlStreamProps props;
  // Type-agnostic byte buffer; libjxl reports the required size in bytes.
  std::vector<uint8_t> pixels;

  for (;;) {
    const JxlDecoderStatus status = JxlDecoderProcessInput(dec);
    if (status == JXL_DEC_ERROR)
      ThrowRDE("JXL: decoding error");
    if (status == JXL_DEC_NEED_MORE_INPUT)
      ThrowRDE("JXL: needs more input (truncated tile?)");
    if (status == JXL_DEC_BASIC_INFO) {
      JxlBasicInfo info = {};
      if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec, &info))
        ThrowRDE("JXL: JxlDecoderGetBasicInfo failed");
      props = {/*width=*/info.xsize,
               /*height=*/info.ysize,
               /*bitsPerSample=*/info.bits_per_sample,
               /*numColorChannels=*/info.num_color_channels,
               /*numExtraChannels=*/info.num_extra_channels};
      if (const JpegXlStreamCheck check =
              checkJpegXlStream(props, cpp, whiteLevel, isFloat);
          check != JpegXlStreamCheck::Ok) {
        ThrowRDE("JXL: %s (codestream %ux%u, %u bit, %u color + %u extra "
                 "channels; DNG cpp %u, WhiteLevel %u)",
                 toString(check), props.width, props.height,
                 props.bitsPerSample, props.numColorChannels,
                 props.numExtraChannels, cpp, whiteLevel);
      }
      continue;
    }
    if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      size_t buf_size = 0;
      if (JXL_DEC_SUCCESS != JxlDecoderImageOutBufferSize(dec, &fmt, &buf_size))
        ThrowRDE("JXL: JxlDecoderImageOutBufferSize failed");
      // copyTile indexes the buffer as a tightly-packed jxl_w * cpp row stride;
      // make libjxl's own accounting confirm that before trusting it.
      if (const size_t needed =
              jpegXlTileBufferBytes(props.width, props.height, cpp, sampleSize);
          buf_size < needed) {
        ThrowRDE("JXL: output buffer of %zu bytes is short of the %zu needed "
                 "for a %ux%u tile of %u channels",
                 buf_size, needed, props.width, props.height, cpp);
      }
      pixels.resize(buf_size);
      if (JXL_DEC_SUCCESS !=
          JxlDecoderSetImageOutBuffer(dec, &fmt, pixels.data(), buf_size))
        ThrowRDE("JXL: JxlDecoderSetImageOutBuffer failed");
      // Must follow SetImageOutBuffer -- that ordering is the libjxl API
      // contract.
      //
      // libjxl's default (JXL_BIT_DEPTH_FROM_PIXEL_FORMAT) stretches the
      // codestream to fill uint16. Whether that is right depends entirely on
      // which range the DNG's WhiteLevel is expressed in, so let
      // jpegXlBitDepthMode decide from WhiteLevel rather than assuming either
      // way; both kinds of file exist. Only override when we want the
      // codestream's own range, since the default needs no call at all.
      if (jpegXlBitDepthMode(props, whiteLevel, isFloat) ==
          JpegXlBitDepthMode::FromCodestream) {
        const JxlBitDepth bitDepth = {/*type=*/JXL_BIT_DEPTH_FROM_CODESTREAM,
                                      /*bits_per_sample=*/0,
                                      /*exponent_bits_per_sample=*/0};
        if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBitDepth(dec, &bitDepth))
          ThrowRDE("JXL: JxlDecoderSetImageOutBitDepth failed");
      }
      continue;
    }
    if (status == JXL_DEC_FULL_IMAGE)
      continue; // image now in `pixels`
    if (status == JXL_DEC_SUCCESS)
      break;
    ThrowRDE("JXL: unexpected decoder status %d", static_cast<int>(status));
  }

  if (pixels.empty())
    ThrowRDE("JXL: no pixel data decoded");

  const JpegXlTileExtent copy = clipJpegXlTile(
      static_cast<uint32_t>(mRaw->dim.x), static_cast<uint32_t>(mRaw->dim.y),
      offX, offY, props.width, props.height);
  if (copy.empty())
    ThrowRDE("JXL: tile origin (%u,%u) lies outside the %ix%i raw image", offX,
             offY, mRaw->dim.x, mRaw->dim.y);

  if (isFloat) {
    copyTile<float>(mRaw->getF32DataAsUncroppedArray2DRef(),
                    reinterpret_cast<const float*>(pixels.data()), props.width,
                    copy.w, copy.h, cpp, offX, offY);
  } else {
    copyTile<uint16_t>(mRaw->getU16DataAsUncroppedArray2DRef(),
                       reinterpret_cast<const uint16_t*>(pixels.data()),
                       props.width, copy.w, copy.h, cpp, offX, offY);
  }
}

} // namespace rawspeed

#else

#pragma message                                                                \
    "JPEG XL is not present! DNG JPEG XL (DNG 1.7) compression will not be "   \
    "supported!"

#endif
