/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
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

#include "decompressors/PanasonicDecompressorV5.h"
#include "common/Mutex.h"                 // for MutexLocker
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include <algorithm>                      // for min
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstring>                        // for memcpy
#include <utility>                        // for move
#include <vector>                         // for vector

namespace rawspeed {

PanasonicDecompressorV5::PanasonicDecompressorV5(const RawImage& img,
                                                 const ByteStream& input_,
                                                 bool zero_is_not_bad,
                                                 uint32 section_split_offset_,
                                                 uint32 bps_)
    : AbstractParallelizedDecompressor(img), zero_is_bad(!zero_is_not_bad),
      section_split_offset(section_split_offset_), bps(bps_),
      encodedDataSize(bps_ == 12 ? 10 : 9) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  // FIXME sanity check sizes
  // this will be quite exciting, as the Panasonic G9 high-resolution mode
  // produces 10480x7794
  // FIXME the whole point is to check for run-away CPU and for DoS on memory;
  // focus on that if (width > 5488 || height > 3912)
  //   ThrowRDE("Too large image size: (%u; %u)", width, height);

  if (SerializationBlockSize < section_split_offset)
    ThrowRDE(
        "Bad section_split_offset: %u, less than SerializationBlockSize (%u)",
        section_split_offset, SerializationBlockSize);

  if (mRaw->dim.x % encodedDataSize != 0)
    ThrowRDE(
        "Raw x dimension does not work for encoded data size given: (%u; %u)",
        mRaw->dim.x, encodedDataSize);

  const auto rawBytesNormal =
      (mRaw->dim.area() * PixelDataBlockSize) / encodedDataSize;

  input = input_.peekStream(rawBytesNormal);
}

struct PanasonicDecompressorV5::DataPump {

  ByteStream input;
  std::vector<uchar8> buf;
  int bufPosition = 0;
  uint32 section_split_offset;

  DataPump(ByteStream input_, int section_split_offset_)
      : input(std::move(input_)), section_split_offset(section_split_offset_) {
    buf.resize(SerializationBlockSize);
  }

  void skipBytes(rawspeed::Buffer::size_type bytes) {

    rawspeed::Buffer::size_type skippableBytes =
        (bytes / SerializationBlockSize) * SerializationBlockSize;
    input.skipBytes(skippableBytes);

    auto emptyReadBytes = bytes - skippableBytes;
    if (emptyReadBytes % PixelDataBlockSize != 0)
      ThrowRDE("Skipping empty read bytes does not align with pixel data block "
               "size; this will lead to failure in decooding: (%u; %u)",
               emptyReadBytes, PixelDataBlockSize);

    // consume the remainder of the bytes which we cannot just blindly skip
    while (emptyReadBytes > 0) {
      /* discard data = */ readBlock();
      emptyReadBytes -= PixelDataBlockSize;
    }
  }

  uchar8* readBlock() {

    if (!bufPosition) {
      assert(SerializationBlockSize >= section_split_offset);
      auto size = std::min(input.getRemainSize(),
                           SerializationBlockSize - section_split_offset);
      memcpy(buf.data() + section_split_offset, input.getData(size), size);

      size = std::min(input.getRemainSize(), section_split_offset);
      if (size != 0)
        memcpy(buf.data(), input.getData(size), size);
    }

    uchar8* blockAddress = buf.data() + bufPosition;

    bufPosition += PixelDataBlockSize;
    bufPosition &= SerializationBlockSizeMask;

    return blockAddress;
  }
};

void PanasonicDecompressorV5::decompressThreaded(
    const RawDecompressorThread* t) const {

  DataPump threadDataPump(input, section_split_offset);

  const auto raw_width = mRaw->dim.x;
  const auto decodeStreamOffset =
      (t->start * raw_width * PixelDataBlockSize) / encodedDataSize;
  threadDataPump.skipBytes(decodeStreamOffset);

  std::vector<uint32> badPixelTracker;
  for (uint32 y = t->start; y < t->end; y++) {

    auto* dest = reinterpret_cast<ushort16*>(mRaw->getData(0, y));

    for (auto col = 0; col < raw_width; col += encodedDataSize) {
      uchar8* bytes = threadDataPump.readBlock();
      if (bps == 12) {
        *dest++ = ((bytes[1] & 0xF) << 8) + bytes[0];
        *dest++ = 16 * bytes[2] + (bytes[1] >> 4);

        *dest++ = ((bytes[4] & 0xF) << 8) + bytes[3];
        *dest++ = 16 * bytes[5] + (bytes[4] >> 4);

        *dest++ = ((bytes[7] & 0xF) << 8) + bytes[6];
        *dest++ = 16 * bytes[8] + (bytes[7] >> 4);

        *dest++ = ((bytes[10] & 0xF) << 8) + bytes[9];
        *dest++ = 16 * bytes[11] + (bytes[10] >> 4);

        *dest++ = ((bytes[13] & 0xF) << 8) + bytes[12];
        *dest++ = 16 * bytes[14] + (bytes[13] >> 4);

        // FIXME zero_pos needs to be filled for bad pixels
      } else if (bps == 14) {
        *dest++ = bytes[0] + ((bytes[1] & 0x3F) << 8);
        *dest++ = (bytes[1] >> 6) + 4 * (bytes[2]) + ((bytes[3] & 0xF) << 10);
        *dest++ = (bytes[3] >> 4) + 16 * (bytes[4]) + ((bytes[5] & 3) << 12);
        *dest++ = ((bytes[5] & 0xFC) >> 2) + (bytes[6] << 6);

        *dest++ = bytes[7] + ((bytes[8] & 0x3F) << 8);
        *dest++ = (bytes[8] >> 6) + 4 * bytes[9] + ((bytes[10] & 0xF) << 10);
        *dest++ = (bytes[10] >> 4) + 16 * bytes[11] + ((bytes[12] & 3) << 12);
        *dest++ = ((bytes[12] & 0xFC) >> 2) + (bytes[13] << 6);

        *dest++ = bytes[14] + ((bytes[15] & 0x3F) << 8);

        // FIXME zero_pos needs to be filled for bad pixels
      }
    }
  }

  if (zero_is_bad && !badPixelTracker.empty()) {
    MutexLocker guard(&mRaw->mBadPixelMutex);
    mRaw->mBadPixelPositions.insert(mRaw->mBadPixelPositions.end(),
                                    badPixelTracker.begin(),
                                    badPixelTracker.end());
  }
}

} // namespace rawspeed
