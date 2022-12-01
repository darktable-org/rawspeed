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

#include "rawspeedconfig.h" // for HAVE_JPEG_MEM_SRC, HAVE_JPEG

#ifdef HAVE_JPEG

#include "adt/Array2DRef.h"               // for Array2DRef
#include "adt/Point.h"                    // for iPoint2D
#include "common/Memory.h"                // for alignedFree, alignedMalloc...
#include "decoders/RawDecoderException.h" // for ThrowException, ThrowRDE
#include "decompressors/JpegDecompressor.h"
#include "io/ByteStream.h" // for ByteStream
#include <algorithm>       // for min, fill_n
#include <array>           // for array
#include <jpeglib.h>       // for jpeg_destroy_decompress
#include <memory>          // for unique_ptr

#ifndef HAVE_JPEG_MEM_SRC
#include "io/IOException.h" // for ThrowIOE
#endif

using std::unique_ptr;
using std::min;

namespace rawspeed {

#ifdef HAVE_JPEG_MEM_SRC

// FIXME: some libjpeg versions discard const qual for the input data pointer
// should this be a cmake check?
#define JPEG_MEMSRC(A, B, C)                                                   \
  jpeg_mem_src(A, const_cast<unsigned char*>(B), C) // NOLINT

#else

#define JPEG_MEMSRC(A, B, C) jpeg_mem_src_int(A, B, C)
/* Read JPEG image from a memory segment */

static void init_source(j_decompress_ptr cinfo) {}
static boolean fill_input_buffer(j_decompress_ptr cinfo) {
  auto* src = (struct jpeg_source_mgr*)cinfo->src;
  return (boolean) !!src->bytes_in_buffer;
}
static void skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
  auto* src = (struct jpeg_source_mgr*)cinfo->src;

  if (num_bytes > (int)src->bytes_in_buffer)
    ThrowIOE("read out of buffer");
  if (num_bytes > 0) {
    src->next_input_byte += (size_t)num_bytes;
    src->bytes_in_buffer -= (size_t)num_bytes;
  }
}
static void term_source(j_decompress_ptr cinfo) {}
static void jpeg_mem_src_int(j_decompress_ptr cinfo,
                             const unsigned char* buffer, long nbytes) {
  struct jpeg_source_mgr* src;

  if (cinfo->src == nullptr) { /* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr*)(*cinfo->mem->alloc_small)(
        (j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(struct jpeg_source_mgr));
  }

  src = (struct jpeg_source_mgr*)cinfo->src;
  src->init_source = init_source;
  src->fill_input_buffer = fill_input_buffer;
  src->skip_input_data = skip_input_data;
  src->resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->term_source = term_source;
  src->bytes_in_buffer = nbytes;
  src->next_input_byte = (const JOCTET*)buffer;
}

#endif

[[noreturn]] METHODDEF(void) my_error_throw(j_common_ptr cinfo) {
  std::array<char, JMSG_LENGTH_MAX> buf;
  buf.fill(0);
  (*cinfo->err->format_message)(cinfo, buf.data());
  ThrowRDE("JPEG decoder error: %s", buf.data());
}

struct JpegDecompressor::JpegDecompressStruct : jpeg_decompress_struct {
  struct jpeg_error_mgr jerr;

  JpegDecompressStruct(const JpegDecompressStruct&) = delete;
  JpegDecompressStruct(JpegDecompressStruct&&) noexcept = delete;
  JpegDecompressStruct&
  operator=(const JpegDecompressStruct&) noexcept = delete;
  JpegDecompressStruct& operator=(JpegDecompressStruct&&) noexcept = delete;

  JpegDecompressStruct() {
    jpeg_create_decompress(this);

    err = jpeg_std_error(&jerr);
    jerr.error_exit = &my_error_throw;
  }
  ~JpegDecompressStruct() { jpeg_destroy_decompress(this); }
};

void JpegDecompressor::decode(uint32_t offX,
                              uint32_t offY) { /* Each slice is a JPEG image */
  struct JpegDecompressStruct dinfo;

  const auto size = input.getRemainSize();

  JPEG_MEMSRC(&dinfo, input.getData(size), size);

  if (JPEG_HEADER_OK != jpeg_read_header(&dinfo, static_cast<boolean>(true)))
    ThrowRDE("Unable to read JPEG header");

  jpeg_start_decompress(&dinfo);
  if (dinfo.output_components != static_cast<int>(mRaw->getCpp()))
    ThrowRDE("Component count doesn't match");
  int row_stride = dinfo.output_width * dinfo.output_components;

  unique_ptr<uint8_t[], // NOLINT
             decltype(&alignedFree)>
      complete_buffer(
          alignedMallocArray<uint8_t, 16>(dinfo.output_height, row_stride),
          &alignedFree);

  const Array2DRef<uint8_t> tmp(&complete_buffer[0],
                                dinfo.output_components * dinfo.output_width,
                                dinfo.output_height, row_stride);

  while (dinfo.output_scanline < dinfo.output_height) {
    JSAMPROW rowOut = &tmp(dinfo.output_scanline, 0);
    if (0 == jpeg_read_scanlines(&dinfo, &rowOut, 1))
      ThrowRDE("JPEG Error while decompressing image.");
  }
  jpeg_finish_decompress(&dinfo);

  // Now the image is decoded, and we copy the image data
  int copy_w = min(mRaw->dim.x - offX, dinfo.output_width);
  int copy_h = min(mRaw->dim.y - offY, dinfo.output_height);

  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());
  for (int row = 0; row < copy_h; row++) {
    for (int col = 0; col < dinfo.output_components * copy_w; col++)
      out(row + offY, dinfo.output_components * offX + col) = tmp(row, col);
  }
}

} // namespace rawspeed

#else

#pragma message                                                                \
    "JPEG is not present! Lossy JPEG compression will not be supported!"

#endif
