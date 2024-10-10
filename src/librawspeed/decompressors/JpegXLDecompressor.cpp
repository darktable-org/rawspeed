/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
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

#include "rawspeedconfig.h" // IWYU pragma: keep

#ifdef HAVE_JXL

#include "adt/Array2DRef.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/JpegXLDecompressor.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <jxl/codestream_header.h>
#include <jxl/decode.h>
#include <jxl/types.h>
#include <vector>

using std::min;

namespace rawspeed {

void JpegXLDecompressor::decode(
    uint32_t offX, uint32_t offY) { /* Each slice is a JPEG XL image */

  JxlSignature signature = JxlSignatureCheck(input.begin(), input.getSize());

  if (signature != JXL_SIG_CODESTREAM && signature != JXL_SIG_CONTAINER)
    ThrowRDE("Unable to verify JPEG XL signature");

  JxlDecoder* decoder = JxlDecoderCreate(nullptr);

  if (!decoder)
    ThrowRDE("Unable to instantiate a JPEG XL decoder");

  if (JxlDecoderSetInput(decoder, input.begin(), input.getSize()) !=
      JXL_DEC_SUCCESS) {
    JxlDecoderDestroy(decoder);
    ThrowRDE("Unable to set input data for JPEG XL decoder");
  }

  if (JxlDecoderSubscribeEvents(decoder,
                                JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE) !=
      JXL_DEC_SUCCESS) {
    JxlDecoderDestroy(decoder);
    ThrowRDE("Unable to subscribe to JPEG XL decoder events");
  }

  JxlDecoderStatus status;
  JxlBasicInfo basicinfo;

  const JxlPixelFormat pixel_format = {
      mRaw->getCpp(),    // number of channels
      JXL_TYPE_UINT16,   // data type
      JXL_NATIVE_ENDIAN, // endianness
      0                  // alignment
  };

  std::vector<uint16_t> complete_buffer;

  // Decoding loop
  while (true) {
    status = JxlDecoderProcessInput(decoder);

    if (status == JXL_DEC_ERROR) {
      JxlDecoderDestroy(decoder);
      ThrowRDE("JPEG XL decoding error");
    }

    if (status == JXL_DEC_NEED_MORE_INPUT) {
      JxlDecoderDestroy(decoder);
      ThrowRDE("JPEG XL stream input data incomplete");
    }

    if (status == JXL_DEC_BASIC_INFO) {
      if (JxlDecoderGetBasicInfo(decoder, &basicinfo) != JXL_DEC_SUCCESS) {
        JxlDecoderDestroy(decoder);
        ThrowRDE("JPEG XL stream basic info not available");
      }

      // Unlikely to happen, but let there be a sanity check
      if (basicinfo.xsize == 0 || basicinfo.ysize == 0) {
        JxlDecoderDestroy(decoder);
        ThrowRDE("JPEG XL image declares zero dimensions");
      }

      if (basicinfo.num_color_channels != pixel_format.num_channels)
        ThrowRDE("Component count doesn't match");

      continue; // go to next loop iteration to process rest of the input
    }

    if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      size_t size =
          basicinfo.xsize * basicinfo.ysize * basicinfo.num_color_channels;
      complete_buffer.resize(size);
      JxlDecoderSetImageOutBuffer(decoder, &pixel_format,
                                  complete_buffer.data(), size);
      continue; // go to next iteration to process rest of the input
    }

    // If the image is an animation, more full frames may be decoded. We do not
    // check and reject the image if it is an animation, but only read the first
    // frame. It hardly makes sense to process such an image.
    if (status == JXL_DEC_FULL_IMAGE)
      break; // Terminate processing

  } // end of processing loop

  JxlDecoderDestroy(decoder);

  const Array2DRef<uint16_t> tmp(complete_buffer.data(),
                                 basicinfo.num_color_channels * basicinfo.xsize,
                                 basicinfo.xsize);

  // Now the image is decoded, and we copy the image data
  unsigned int copy_w = min(mRaw->dim.x - offX, basicinfo.xsize);
  unsigned int copy_h = min(mRaw->dim.y - offY, basicinfo.ysize);

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  for (unsigned int row = 0; row < copy_h; row++) {
    for (unsigned int col = 0; col < basicinfo.num_color_channels * copy_w;
         col++)
      out(offY + row, basicinfo.num_color_channels * offX + col) =
          tmp(row, col);
  }
}

} // namespace rawspeed

#else

#pragma message                                                                \
    "JPEG XL is not present! JPEG XL compression will not be supported!"

#endif
