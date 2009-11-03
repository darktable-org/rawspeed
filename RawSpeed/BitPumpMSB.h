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

namespace RawSpeed {

// Note: Allocated buffer MUST be at least size+sizeof(guint) large.

class BitPumpMSB
{
public:
  BitPumpMSB(ByteStream *s);
  BitPumpMSB(const guchar* _buffer, guint _size );
	guint getBitsSafe(guint nbits);
	guint getBitSafe();
	guchar getByteSafe();
	void setAbsoluteOffset(guint offset);     // Set offset in bytes
  __inline guint getOffset() { return off-(mLeft>>3);}
  __inline void checkPos()  { if (off>size) throw IOException("Out of buffer read");};        // Check if we have a valid position
  __inline guint getBitNoFill() {return (mCurr >> (--mLeft)) & 1;}
  __inline guint peekByteNoFill() {return ((mCurr >> (mLeft-8))) & 0xff; }
  __inline guint getBitsNoFill(guint nbits) {return ((mCurr >> (mLeft -= (nbits)))) & masks[nbits];}
  __inline guint peekBitsNoFill(guint nbits) {return ((mCurr >> (mLeft-nbits))) &masks[nbits]; }
  __inline void fill();  // Fill the buffer with at least 24 bits

  __inline guint BitPumpMSB::getBit() {
    if (!mLeft) fill();

    return (mCurr >> (--mLeft)) & 1;
  }

  __inline guint BitPumpMSB::getBits(guint nbits) {
    if (mLeft < nbits) {
      if (nbits>24)
        throw IOException("Invalid data, attempting to read more than 24 bits.");

      fill();
    }

    return ((mCurr >> (mLeft -= (nbits)))) & masks[nbits];
  }

  __inline guint BitPumpMSB::peekBit() {
    if (!mLeft) fill();

    return (mCurr >> (mLeft - 1)) & 1;
  }

  __inline guint BitPumpMSB::peekBits(guint nbits) {
    if (mLeft < nbits) {
      if (nbits>24)
        throw IOException("Invalid data, attempting to read more than 24 bits.");

      fill();
    }

    return ((mCurr >> (mLeft - nbits))) & masks[nbits];
  }

  __inline guint BitPumpMSB::peekByte() {
    if (mLeft < 8) {
      fill();
    }

    if (off > size)
      throw IOException("Out of buffer read");

    return ((mCurr >> (mLeft - 8))) & 0xff;
  }

  __inline void BitPumpMSB::skipBits(unsigned int nbits) {
    if (mLeft < nbits) {
      fill();

      if (off > size)
        throw IOException("Out of buffer read");
    }

    mLeft -= nbits;
  }

  __inline void BitPumpMSB::skipBitsNoFill(unsigned int nbits) {
    mLeft -= nbits;
  }

  __inline unsigned char BitPumpMSB::getByte() {
    if (mLeft < 8) {
      fill();
    }

    return ((mCurr >> (mLeft -= 8))) & 0xff;
  }

  virtual ~BitPumpMSB(void);
protected:
  void __inline init();
  const guchar* buffer;
  const guint size;            // This if the end of buffer.
	guint masks[31];
  guint mLeft;
  guint mCurr;
  guint off;                  // Offset in bytes
private:
};

} // namespace RawSpeed
