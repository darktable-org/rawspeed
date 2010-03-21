/* 
    RawSpeed - RAW file decoder.

    Copyright (C) 2009 Klaus Post

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

    http://www.klauspost.com
*/
#pragma once
#include "ByteStream.h"
#include "IOException.h"

namespace RawSpeed {

// Note: Allocated buffer MUST be at least size+sizeof(guint) large.

class BitPumpJPEG
{
public:
  BitPumpJPEG(ByteStream *s);
  BitPumpJPEG(const guchar* _buffer, guint _size );
	guint getBits(guint nbits);
	guint getBit();
	guint getBitsSafe(guint nbits);
	guint getBitSafe();
	guint peekBits(guint nbits);
	guint peekBit();
  guint peekByte();
  void skipBits(guint nbits);
  __inline void skipBitsNoFill(guint nbits){ mLeft -= nbits; }
  __inline void checkPos()  { if (off>size) throw IOException("Out of buffer read");};        // Check if we have a valid position
	guchar getByte();
	guchar getByteSafe();
	void setAbsoluteOffset(guint offset);     // Set offset in bytes
  guint getOffset() { return off-(mLeft>>3)+stuffed;}
  __inline guint getBitNoFill() {return (mCurr >> (--mLeft)) & 1;}
  __inline guint peekByteNoFill() {return ((mCurr >> (mLeft-8))) & 0xff; }
  __inline guint peekBitsNoFill(guint nbits) {return ((mCurr >> (mLeft-nbits))) & ((1 << nbits) - 1); }
  __inline guint getBitsNoFill(guint nbits) { return ((mCurr >> (mLeft -= (nbits)))) & ((1 << nbits) - 1);}

#define TEST_IF_FF(VAL) if (VAL == 0xFF) {\
  if (buffer[off] == 0)\
  off++;\
  else  {\
  VAL = 0;off--;stuffed++;\
  }\
  }


  // Fill the buffer with at least 24 bits
  void __inline BitPumpJPEG::fill() {
    guchar c, c2, c3;

    int m = mLeft >> 3;

    if (mLeft > 23)
      return;

    if (m == 2)
    {
      // 16 to 23 bits left, we can add 1 byte
      c = buffer[off++];
      TEST_IF_FF(c);
      mCurr = (mCurr << 8) | c;
      mLeft += 8;
      return;
    }

    if (m == 1)
    {
      // 8 to 15 bits left, we can add 2 bytes
      c = buffer[off++];
      TEST_IF_FF(c);
      c2 = buffer[off++];
      TEST_IF_FF(c2);
      mCurr = (mCurr << 16) | (c<<8) | c2;
      mLeft += 16;
      return;
    }

    // 0 to 7 bits left, we can add 3 bytes
    c = buffer[off++];
    TEST_IF_FF(c);
    c2 = buffer[off++];
    TEST_IF_FF(c2);
    c3 = buffer[off++];
    TEST_IF_FF(c3);
    mCurr = (mCurr << 24) | (c<<16) | (c2<<8) | c3;
    mLeft += 24;
  }

#undef TEST_IF_FF

  virtual ~BitPumpJPEG(void);
protected:
  void __inline init();
  const guchar* buffer;
  guint mLeft;
  guint off;                  // Offset in bytes
  guint mCurr;
  guint stuffed;              // How many bytes has been stuffed?
  const guint size;            // This if the end of buffer.
private:
};

} // namespace RawSpeed
