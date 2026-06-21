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
#include "decoders/RawDecoderException.h"
#include "decompressors/JpegXlDecompressor.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <jxl/decode.h>
#include <vector>

using std::min;

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
  const JxlPixelFormat fmt = {/*num_channels=*/cpp,
                              /*data_type=*/JXL_TYPE_UINT16,
                              /*endianness=*/JXL_LITTLE_ENDIAN,
                              /*align=*/0};

  JxlBasicInfo info = {};
  uint32_t jxl_w = 0;
  uint32_t jxl_h = 0;
  std::vector<uint16_t> pixels;

  for (;;) {
    const JxlDecoderStatus status = JxlDecoderProcessInput(dec);
    if (status == JXL_DEC_ERROR)
      ThrowRDE("JXL: decoding error");
    if (status == JXL_DEC_NEED_MORE_INPUT)
      ThrowRDE("JXL: needs more input (truncated tile?)");
    if (status == JXL_DEC_BASIC_INFO) {
      if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec, &info))
        ThrowRDE("JXL: JxlDecoderGetBasicInfo failed");
      jxl_w = info.xsize;
      jxl_h = info.ysize;
      if (info.num_color_channels != cpp)
        ThrowRDE("JXL: color channel count %u does not match cpp %u",
                 info.num_color_channels, cpp);
      continue;
    }
    if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      size_t buf_size = 0;
      if (JXL_DEC_SUCCESS != JxlDecoderImageOutBufferSize(dec, &fmt, &buf_size))
        ThrowRDE("JXL: JxlDecoderImageOutBufferSize failed");
      pixels.resize(buf_size / sizeof(uint16_t));
      if (JXL_DEC_SUCCESS !=
          JxlDecoderSetImageOutBuffer(dec, &fmt, pixels.data(), buf_size))
        ThrowRDE("JXL: JxlDecoderSetImageOutBuffer failed");
      continue;
    }
    if (status == JXL_DEC_FULL_IMAGE)
      continue; // image now in `pixels`
    if (status == JXL_DEC_SUCCESS)
      break;
    ThrowRDE("JXL: unexpected decoder status %d", static_cast<int>(status));
  }

  if (pixels.empty() || jxl_w == 0 || jxl_h == 0)
    ThrowRDE("JXL: no pixel data decoded");

  // Copy the decoded interleaved uint16 tile into the raw buffer at
  // (offX,offY).
  const uint32_t copy_w = min(static_cast<uint32_t>(mRaw->dim.x) - offX, jxl_w);
  const uint32_t copy_h = min(static_cast<uint32_t>(mRaw->dim.y) - offY, jxl_h);

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  for (uint32_t row = 0; row < copy_h; ++row) {
    for (uint32_t col = 0; col < cpp * copy_w; ++col) {
      out(static_cast<int>(row + offY), static_cast<int>(cpp * offX + col)) =
          pixels[(static_cast<size_t>(row) * jxl_w * cpp) + col];
    }
  }
}

} // namespace rawspeed

#else

#pragma message                                                                \
    "JPEG XL is not present! DNG JPEG XL (DNG 1.7) compression will not be "   \
    "supported!"

#endif
