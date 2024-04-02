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

#ifdef HAVE_JPEG

#include "adt/AlignedAllocator.h"
#include "adt/Array2DRef.h"
#include "adt/Point.h"
#include "decoders/RawDecoderException.h"
#include "decompressors/JpegDecompressor.h"
#include "io/IOException.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <jpeglib.h>
#include <memory>
#include <vector>

using std::min;
using std::unique_ptr;

namespace rawspeed {

namespace {

/* Read JPEG image from a memory segment */

void init_source(j_decompress_ptr /*cinfo*/) {
  // No action needed.
}

boolean fill_input_buffer(j_decompress_ptr cinfo) {
  return cinfo->src->bytes_in_buffer != 0;
}

// NOLINTNEXTLINE(google-runtime-int)
void skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
  auto* src = cinfo->src;

  if (num_bytes > static_cast<int>(src->bytes_in_buffer))
    ThrowIOE("read out of buffer");
  if (num_bytes > 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wunknown-warning-option"
#pragma GCC diagnostic ignored "-Wunsafe-buffer-usage"
    src->next_input_byte += static_cast<size_t>(num_bytes);
#pragma GCC diagnostic pop
    src->bytes_in_buffer -= static_cast<size_t>(num_bytes);
  }
}

void term_source(j_decompress_ptr /*cinfo*/) {
  // No action needed.
}

[[maybe_unused]] void
jpeg_mem_src_int(j_decompress_ptr cinfo, const unsigned char* buffer,
                 long nbytes) { // NOLINT(google-runtime-int)
  jpeg_source_mgr* src;

  if (cinfo->src == nullptr) { /* first time for this JPEG object? */
    void* buf =
        cinfo->mem->alloc_small(reinterpret_cast<j_common_ptr>(cinfo),
                                JPOOL_PERMANENT, sizeof(jpeg_source_mgr));
    cinfo->src = static_cast<jpeg_source_mgr*>(buf);
  }

  src = cinfo->src;
  src->init_source = init_source;
  src->fill_input_buffer = fill_input_buffer;
  src->skip_input_data = skip_input_data;
  src->resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->term_source = term_source;
  src->bytes_in_buffer = nbytes;
  src->next_input_byte = buffer;
}

// NOLINTNEXTLINE(readability-static-definition-in-anonymous-namespace)
[[noreturn]] METHODDEF(void) my_error_throw(j_common_ptr cinfo) {
  std::array<char, JMSG_LENGTH_MAX> buf;
  buf.fill(0);
  cinfo->err->format_message(cinfo, buf.data());
  ThrowRDE("JPEG decoder error: %s", buf.data());
}

} // namespace

struct JpegDecompressor::JpegDecompressStruct final : jpeg_decompress_struct {
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
  JpegDecompressStruct dinfo;

#ifdef HAVE_JPEG_MEM_SRC
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast): why, macOS?!
  jpeg_mem_src(&dinfo, const_cast<uint8_t*>(input.begin()), input.getSize());
#else
  jpeg_mem_src_int(&dinfo, input.begin(), input.getSize());
#endif

  if (JPEG_HEADER_OK != jpeg_read_header(&dinfo, static_cast<boolean>(true)))
    ThrowRDE("Unable to read JPEG header");

  jpeg_start_decompress(&dinfo);
  if (dinfo.output_components != static_cast<int>(mRaw->getCpp()))
    ThrowRDE("Component count doesn't match");
  int row_stride = dinfo.output_width * dinfo.output_components;

  std::vector<uint8_t, AlignedAllocator<uint8_t, 16>> complete_buffer;
  complete_buffer.resize(dinfo.output_height * row_stride);

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
