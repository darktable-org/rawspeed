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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

    http://www.klauspost.com
*/
#pragma once
#include "ByteStream.h"

// Note: Allocated buffer MUST be at least size+sizeof(guint) large.


class BitPumpPlain
{
public:
  BitPumpPlain(ByteStream *s);
  BitPumpPlain(const guchar* _buffer, guint _size );
	guint getBits(guint nbits);
	guint getBit();
	guint getBitsSafe(guint nbits);
	guint getBitSafe();
	guint peekBits(guint nbits);
	guint peekBit();
  guint peekByte();
  void skipBits(guint nbits);
	guchar getByte();
	guchar getByteSafe();
	void setAbsoluteOffset(guint offset);
  guint getOffset() { return off>>3;}
  __inline void checkPos()  { if (off>size) throw IOException("Out of buffer read");};        // Check if we have a valid position

  virtual ~BitPumpPlain(void);
protected:
  const guchar* buffer;
  const guint size;            // This if the end of buffer.
  guint off;                  // Offset in bytes
	guint masks[31];
private:
};


