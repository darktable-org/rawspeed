/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real

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

#include "decompressors/UncompressedDecompressor.h"
#include "common/Common.h"                // for uint32, uchar8, ushort16
#include "common/Point.h"                 // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpMSB.h"                // for BitPumpMSB
#include "io/BitPumpMSB16.h"              // for BitPumpMSB16
#include "io/BitPumpMSB32.h"              // for BitPumpMSB32
#include "io/BitPumpPlain.h"              // for BitPumpPlain
#include "io/ByteStream.h"                // for ByteStream
#include "io/Endianness.h"                // for getHostEndianness, Endiann...
#include "io/IOException.h"               // for ThrowIOE
#include <algorithm>                      // for min

using namespace std;

namespace RawSpeed {

void UncompressedDecompressor::readUncompressedRaw(iPoint2D& size,
                                                   iPoint2D& offset,
                                                   int inputPitch,
                                                   int bitPerPixel,
                                                   BitOrder order) {
  uchar8* data = mRaw->getData();
  uint32 outPitch = mRaw->pitch;
  uint64 w = size.x;
  uint64 h = size.y;
  uint32 cpp = mRaw->getCpp();
  uint64 ox = offset.x;
  uint64 oy = offset.y;

  if (input.getRemainSize() < (inputPitch * h)) {
    if ((int)input.getRemainSize() > inputPitch) {
      h = input.getRemainSize() / inputPitch - 1;
      mRaw->setError("Image truncated (file is too short)");
    } else
      ThrowIOE("readUncompressedRaw: Not enough data to decode a single line. "
               "Image file truncated.");
  }
  if (bitPerPixel > 16 && mRaw->getDataType() == TYPE_USHORT16)
    ThrowRDE("readUncompressedRaw: Unsupported bit depth");

  uint32 skipBits = inputPitch - w * cpp * bitPerPixel / 8; // Skip per line
  if (oy > (uint64)mRaw->dim.y)
    ThrowRDE("readUncompressedRaw: Invalid y offset");
  if (ox + size.x > (uint64)mRaw->dim.x)
    ThrowRDE("readUncompressedRaw: Invalid x offset");

  uint64 y = oy;
  h = min(h + oy, (uint64)mRaw->dim.y);

  if (mRaw->getDataType() == TYPE_FLOAT32) {
    if (bitPerPixel != 32)
      ThrowRDE("readUncompressedRaw: Only 32 bit float point supported");
    copyPixels(&data[offset.x * sizeof(float) * cpp + y * outPitch], outPitch,
               input.getData(inputPitch * (h - y)), inputPitch,
               w * mRaw->getBpp(), h - y);
    return;
  }

  if (BitOrder_Jpeg == order) {
    BitPumpMSB bits(input);
    w *= cpp;
    for (; y < h; y++) {
      auto* dest =
          (ushort16*)&data[offset.x * sizeof(ushort16) * cpp + y * outPitch];
      bits.checkPos();
      for (uint32 x = 0; x < w; x++) {
        uint32 b = bits.getBits(bitPerPixel);
        dest[x] = b;
      }
      bits.skipBits(skipBits);
    }
  } else if (BitOrder_Jpeg16 == order) {
    BitPumpMSB16 bits(input);
    w *= cpp;
    for (; y < h; y++) {
      auto* dest =
          (ushort16*)&data[offset.x * sizeof(ushort16) * cpp + y * outPitch];
      bits.checkPos();
      for (uint32 x = 0; x < w; x++) {
        uint32 b = bits.getBits(bitPerPixel);
        dest[x] = b;
      }
      bits.skipBits(skipBits);
    }
  } else if (BitOrder_Jpeg32 == order) {
    BitPumpMSB32 bits(input);
    w *= cpp;
    for (; y < h; y++) {
      auto* dest =
          (ushort16*)&data[offset.x * sizeof(ushort16) * cpp + y * outPitch];
      bits.checkPos();
      for (uint32 x = 0; x < w; x++) {
        uint32 b = bits.getBits(bitPerPixel);
        dest[x] = b;
      }
      bits.skipBits(skipBits);
    }
  } else {
    if (bitPerPixel == 16 && getHostEndianness() == little) {
      copyPixels(&data[offset.x * sizeof(ushort16) * cpp + y * outPitch],
                 outPitch, input.getData(inputPitch * (h - y)), inputPitch,
                 w * mRaw->getBpp(), h - y);
      return;
    }
    if (bitPerPixel == 12 && (int)w == inputPitch * 8 / 12 &&
        getHostEndianness() == little) {
      decode12BitRaw(w, h);
      return;
    }
    BitPumpPlain bits(input);
    w *= cpp;
    for (; y < h; y++) {
      auto* dest = (ushort16*)&data[offset.x * sizeof(ushort16) + y * outPitch];
      bits.checkPos();
      for (uint32 x = 0; x < w; x++) {
        uint32 b = bits.getBits(bitPerPixel);
        dest[x] = b;
      }
      bits.skipBits(skipBits);
    }
  }
}

void UncompressedDecompressor::decode8BitRaw(uint32 w, uint32 h) {
  if (input.getRemainSize() < w * h) {
    if ((uint32)input.getRemainSize() > w) {
      h = input.getRemainSize() / w - 1;
      mRaw->setError("Image truncated (file is too short)");
    } else
      ThrowIOE("decode8BitRaw: Not enough data to decode a single line. Image "
               "file truncated.");
  }

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(w * h);
  uint32 random = 0;
  for (uint32 y = 0; y < h; y++) {
    auto* dest = (ushort16*)&data[y * pitch];
    for (uint32 x = 0; x < w; x += 1) {
      if (uncorrectedRawValues)
        dest[x] = *in++;
      else
        mRaw->setWithLookUp(*in++, (uchar8*)&dest[x], &random);
    }
  }
}

void UncompressedDecompressor::decode12BitRaw(uint32 w, uint32 h) {
  if (w < 2)
    ThrowIOE("Are you mad? 1 pixel wide raw images are no fun");

  if (input.getRemainSize() < ((w * 12 / 8) * h)) {
    if ((uint32)input.getRemainSize() > (w * 12 / 8)) {
      h = input.getRemainSize() / (w * 12 / 8) - 1;
      mRaw->setError("Image truncated (file is too short)");
    } else
      ThrowIOE("readUncompressedRaw: Not enough data to decode a single line. "
               "Image file truncated.");
  }

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(w * 12 / 8 * h);

  for (uint32 y = 0; y < h; y++) {
    auto* dest = (ushort16*)&data[y * pitch];
    for (uint32 x = 0; x < w; x += 2) {
      uint32 g1 = *in++;
      uint32 g2 = *in++;
      dest[x] = g1 | ((g2 & 0xf) << 8);
      uint32 g3 = *in++;
      dest[x + 1] = (g2 >> 4) | (g3 << 4);
    }
  }
}

void UncompressedDecompressor::decode12BitRawWithControl(uint32 w, uint32 h) {
  if (w < 2)
    ThrowIOE("Are you mad? 1 pixel wide raw images are no fun");

  // Calulate expected bytes per line.
  uint32 perline = (w * 12 / 8);
  // Add skips every 10 pixels
  perline += ((w + 2) / 10);

  // If file is too short, only decode as many lines as we have
  if (input.getRemainSize() <= perline) {
    ThrowIOE("decode12BitRawBEWithControl: Not enough data to decode a single "
             "line. Image file truncated.");
  } else if (input.getRemainSize() < (perline * h)) {
    h = input.getRemainSize() / perline - 1;
    mRaw->setError("Image truncated (file is too short)");
  }

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(perline * h);

  uint32 x;
  for (uint32 y = 0; y < h; y++) {
    auto* dest = (ushort16*)&data[y * pitch];
    for (x = 0; x < w; x += 2) {
      uint32 g1 = *in++;
      uint32 g2 = *in++;
      dest[x] = g1 | ((g2 & 0xf) << 8);
      uint32 g3 = *in++;
      dest[x + 1] = (g2 >> 4) | (g3 << 4);
      if ((x % 10) == 8)
        in++;
    }
  }
}

void UncompressedDecompressor::decode12BitRawBEWithControl(uint32 w, uint32 h) {
  if (w < 2)
    ThrowIOE("Are you mad? 1 pixel wide raw images are no fun");

  // Calulate expected bytes per line.
  uint32 perline = (w * 12 / 8);
  // Add skips every 10 pixels
  perline += ((w + 2) / 10);

  // If file is too short, only decode as many lines as we have
  if (input.getRemainSize() <= perline) {
    ThrowIOE("decode12BitRawBEWithControl: Not enough data to decode a single "
             "line. Image file truncated.");
  } else if (input.getRemainSize() < (perline * h)) {
    h = input.getRemainSize() / perline - 1;
    mRaw->setError("Image truncated (file is too short)");
  }

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(perline * h);

  for (uint32 y = 0; y < h; y++) {
    auto* dest = (ushort16*)&data[y * pitch];
    for (uint32 x = 0; x < w; x += 2) {
      uint32 g1 = *in++;
      uint32 g2 = *in++;
      dest[x] = (g1 << 4) | (g2 >> 4);
      uint32 g3 = *in++;
      dest[x + 1] = ((g2 & 0x0f) << 8) | g3;
      if ((x % 10) == 8)
        in++;
    }
  }
}

void UncompressedDecompressor::decode12BitRawBE(uint32 w, uint32 h) {
  if (w < 2)
    ThrowIOE("Are you mad? 1 pixel wide raw images are no fun");

  if (input.getRemainSize() < ((w * 12 / 8) * h)) {
    if ((uint32)input.getRemainSize() > (w * 12 / 8)) {
      h = input.getRemainSize() / (w * 12 / 8) - 1;
      mRaw->setError("Image truncated (file is too short)");
    } else
      ThrowIOE("readUncompressedRaw: Not enough data to decode a single line. "
               "Image file truncated.");
  }

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(w * 12 / 8 * h);

  for (uint32 y = 0; y < h; y++) {
    auto* dest = (ushort16*)&data[y * pitch];
    for (uint32 x = 0; x < w; x += 2) {
      uint32 g1 = *in++;
      uint32 g2 = *in++;
      dest[x] = (g1 << 4) | (g2 >> 4);
      uint32 g3 = *in++;
      dest[x + 1] = ((g2 & 0x0f) << 8) | g3;
    }
  }
}

void UncompressedDecompressor::decode12BitRawBEInterlaced(uint32 w, uint32 h) {
  if (w < 2)
    ThrowIOE("Are you mad? 1 pixel wide raw images are no fun");

  if (input.getRemainSize() < ((w * 12 / 8) * h)) {
    if ((uint32)input.getRemainSize() > (w * 12 / 8)) {
      h = input.getRemainSize() / (w * 12 / 8) - 1;
      mRaw->setError("Image truncated (file is too short)");
    } else
      ThrowIOE("readUncompressedRaw: Not enough data to decode a single line. "
               "Image file truncated.");
  }

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.peekData((w * 12 / 8) * h);
  uint32 half = (h + 1) >> 1;
  for (uint32 row = 0; row < h; row++) {
    uint32 y = row % half * 2 + row / half;
    auto* dest = (ushort16*)&data[y * pitch];
    if (y == 1) {
      // The second field starts at a 2048 byte aligment
      uint32 offset = ((half * w * 3 / 2 >> 11) + 1) << 11;
      if (offset > input.getRemainSize())
        ThrowIOE("Decode12BitSplitRaw: Trying to jump to invalid offset %d",
                 offset);
      in = input.peekData(input.getRemainSize()) + offset;
    }
    for (uint32 x = 0; x < w; x += 2) {
      uint32 g1 = *in++;
      uint32 g2 = *in++;
      dest[x] = (g1 << 4) | (g2 >> 4);
      uint32 g3 = *in++;
      dest[x + 1] = ((g2 & 0x0f) << 8) | g3;
    }
  }
  input.skipBytes(input.getRemainSize());
}

void UncompressedDecompressor::decode12BitRawBEunpacked(uint32 w, uint32 h) {
  if (input.getRemainSize() < w * h * 2) {
    if ((uint32)input.getRemainSize() > w * 2) {
      h = input.getRemainSize() / (w * 2) - 1;
      mRaw->setError("Image truncated (file is too short)");
    } else
      ThrowIOE("readUncompressedRaw: Not enough data to decode a single line. "
               "Image file truncated.");
  }

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(w * h * 2);

  for (uint32 y = 0; y < h; y++) {
    auto* dest = (ushort16*)&data[y * pitch];
    for (uint32 x = 0; x < w; x += 1) {
      uint32 g1 = *in++;
      uint32 g2 = *in++;
      dest[x] = ((g1 & 0x0f) << 8) | g2;
    }
  }
}

void UncompressedDecompressor::decode12BitRawBEunpackedLeftAligned(uint32 w,
                                                                   uint32 h) {
  if (input.getRemainSize() < w * h * 2) {
    if ((uint32)input.getRemainSize() > w * 2) {
      h = input.getRemainSize() / (w * 2) - 1;
      mRaw->setError("Image truncated (file is too short)");
    } else
      ThrowIOE("readUncompressedRaw: Not enough data to decode a single line. "
               "Image file truncated.");
  }

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(w * h * 2);

  for (uint32 y = 0; y < h; y++) {
    auto* dest = (ushort16*)&data[y * pitch];
    for (uint32 x = 0; x < w; x += 1) {
      uint32 g1 = *in++;
      uint32 g2 = *in++;
      dest[x] = (((g1 << 8) | (g2 & 0xf0)) >> 4);
    }
  }
}

void UncompressedDecompressor::decode14BitRawBEunpacked(uint32 w, uint32 h) {
  if (input.getRemainSize() < w * h * 2) {
    if ((uint32)input.getRemainSize() > w * 2) {
      h = input.getRemainSize() / (w * 2) - 1;
      mRaw->setError("Image truncated (file is too short)");
    } else
      ThrowIOE("readUncompressedRaw: Not enough data to decode a single line. "
               "Image file truncated.");
  }

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(w * h * 2);

  for (uint32 y = 0; y < h; y++) {
    auto* dest = (ushort16*)&data[y * pitch];
    for (uint32 x = 0; x < w; x += 1) {
      uint32 g1 = *in++;
      uint32 g2 = *in++;
      dest[x] = ((g1 & 0x3f) << 8) | g2;
    }
  }
}

void UncompressedDecompressor::decode16BitRawUnpacked(uint32 w, uint32 h) {
  if (input.getRemainSize() < w * h * 2) {
    if ((uint32)input.getRemainSize() > w * 2) {
      h = input.getRemainSize() / (w * 2) - 1;
      mRaw->setError("Image truncated (file is too short)");
    } else
      ThrowIOE("readUncompressedRaw: Not enough data to decode a single line. "
               "Image file truncated.");
  }

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(w * h * 2);

  for (uint32 y = 0; y < h; y++) {
    auto* dest = (ushort16*)&data[y * pitch];
    for (uint32 x = 0; x < w; x += 1) {
      uint32 g1 = *in++;
      uint32 g2 = *in++;
      dest[x] = (g2 << 8) | g1;
    }
  }
}

void UncompressedDecompressor::decode16BitRawBEunpacked(uint32 w, uint32 h) {
  if (input.getRemainSize() < w * h * 2) {
    if ((uint32)input.getRemainSize() > w * 2) {
      h = input.getRemainSize() / (w * 2) - 1;
      mRaw->setError("Image truncated (file is too short)");
    } else
      ThrowIOE("readUncompressedRaw: Not enough data to decode a single line. "
               "Image file truncated.");
  }

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(w * h * 2);

  for (uint32 y = 0; y < h; y++) {
    auto* dest = (ushort16*)&data[y * pitch];
    for (uint32 x = 0; x < w; x += 1) {
      uint32 g1 = *in++;
      uint32 g2 = *in++;
      dest[x] = (g1 << 8) | g2;
    }
  }
}

void UncompressedDecompressor::decode12BitRawUnpacked(uint32 w, uint32 h) {
  if (input.getRemainSize() < w * h * 2) {
    if ((uint32)input.getRemainSize() > w * 2) {
      h = input.getRemainSize() / (w * 2) - 1;
      mRaw->setError("Image truncated (file is too short)");
    } else
      ThrowIOE("readUncompressedRaw: Not enough data to decode a single line. "
               "Image file truncated.");
  }

  uchar8* data = mRaw->getData();
  uint32 pitch = mRaw->pitch;
  const uchar8* in = input.getData(w * h * 2);

  for (uint32 y = 0; y < h; y++) {
    auto* dest = (ushort16*)&data[y * pitch];
    for (uint32 x = 0; x < w; x += 1) {
      uint32 g1 = *in++;
      uint32 g2 = *in++;
      dest[x] = ((g2 << 8) | g1) >> 4;
    }
  }
}

} // namespace RawSpeed
