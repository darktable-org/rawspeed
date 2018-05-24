/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Alexey Danilchenko
    Copyright (C) 2018 Stefan Hoffmeister
    Copyright (C) 2018 Roman Lebedev

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
#include <memory>                         // for uninitialized_copy
#include <utility>                        // for move
#include <vector>                         // for vector

namespace rawspeed {

PanasonicDecompressorV5::PanasonicDecompressorV5(const RawImage& img,
                                                 const ByteStream& input_,
                                                 uint32 bps_)
    : mRaw(img), bps(bps_) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != 2)
    ThrowRDE("Unexpected component count / data type");

  switch (bps) {
  case 12:
    pixelsPerPacket = 10;
    break;
  case 14:
    pixelsPerPacket = 9;
    break;
  default:
    ThrowRDE("Unsupported bps: %u", bps);
  }

  if (mRaw->dim.x % pixelsPerPacket != 0)
    ThrowRDE("Image width is not a multiple of pixels-per-packet");

  assert(mRaw->dim.area() % pixelsPerPacket == 0);
  auto inputSize = (mRaw->dim.area() / pixelsPerPacket) * bytesPerPacket;
  inputSize = roundUp(inputSize, BlockSize);
  input = input_.peekStream(inputSize);
}

class PanasonicDecompressorV5::DataPump {
  ByteStream blocks;
  std::vector<uchar8> buf;
  ByteStream input;

  void parseBlock() {
    if (input.getRemainSize() > 0)
      return;

    assert(buf.size() == BlockSize);
    static_assert(BlockSize > section_split_offset, "");

    ByteStream thisBlock = blocks.getStream(BlockSize);

    Buffer FirstSection = thisBlock.getBuffer(section_split_offset);
    Buffer SecondSection = thisBlock.getBuffer(thisBlock.getRemainSize());
    assert(FirstSection.getSize() < SecondSection.getSize());

    // First copy the second section. This makes it the first section.
    auto bufDst = buf.begin();
    bufDst = std::uninitialized_copy(SecondSection.begin(), SecondSection.end(),
                                     bufDst);
    // Now append the original 1'st section right after the new 1'st section.
    bufDst = std::uninitialized_copy(FirstSection.begin(), FirstSection.end(),
                                     bufDst);
    assert(buf.end() == bufDst);

    assert(thisBlock.getRemainSize() == 0);

    // And reset the clock.
    input = ByteStream(DataBuffer(Buffer(buf.data(), buf.size())));
  }

public:
  explicit DataPump(ByteStream blocks_) : blocks(std::move(blocks_)) {
    static_assert(BlockSize % bytesPerPacket == 0, "");
    buf.resize(BlockSize);
  }

  const uchar8* readBlock() {
    parseBlock();
    return input.getData(bytesPerPacket);
  }
};

void PanasonicDecompressorV5::decompress() const {
  DataPump pump(input);

  for (uint32 y = 0; y < static_cast<uint32>(mRaw->dim.y); y++) {
    auto* dest = reinterpret_cast<ushort16*>(mRaw->getData(0, y));

    assert(mRaw->dim.x % pixelsPerPacket == 0);
    for (auto x = 0; x < mRaw->dim.x; x += pixelsPerPacket) {
      const uchar8* bytes = pump.readBlock();

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

        // FIXME badPixelTracker needs to be filled in case of bad pixels
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

        // FIXME badPixelTracker needs to be filled in case of bad pixels
      }
    }
  }
}

} // namespace rawspeed
