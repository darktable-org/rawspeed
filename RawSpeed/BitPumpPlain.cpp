#include "StdAfx.h"
#include "BitPumpPlain.h"
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

namespace RawSpeed {

/*** Used for entropy encoded sections ***/

#define BITS_PER_LONG (8*sizeof(guint))
#define MIN_GET_BITS  (BITS_PER_LONG-7)    /* max value for long getBuffer */


BitPumpPlain::BitPumpPlain(ByteStream *s):
    buffer(s->getData()), size(8*s->getRemainSize()), off(0) {
}

BitPumpPlain::BitPumpPlain(const guchar* _buffer, guint _size) :
    buffer(_buffer), size(_size*8), off(0) {
}

guint BitPumpPlain::getBit() {
  guint v = *(guint*) & buffer[off>>3] >> (off & 7) & 1;
  off++;
  return v;
}

guint BitPumpPlain::getBits(guint nbits) {
  guint v = *(guint*) & buffer[off>>3] >> (off & 7) & ((1 << nbits) - 1);
  off += nbits;
  return v;
}

guint BitPumpPlain::peekBit() {
  return *(guint*)&buffer[off>>3] >> (off&7) & 1;
}

guint BitPumpPlain::peekBits(guint nbits) {
  return *(guint*)&buffer[off>>3] >> (off&7) & ((1 << nbits) - 1);
}

guint BitPumpPlain::peekByte() {
  return *(guint*)&buffer[off>>3] >> (off&7) & 0xff;
}

guint BitPumpPlain::getBitSafe() {
  checkPos();
  return *(guint*)&buffer[off>>3] >> (off&7) & 1;
}

guint BitPumpPlain::getBitsSafe(unsigned int nbits) {
  checkPos();
  return *(guint*)&buffer[off>>3] >> (off&7) & ((1 << nbits) - 1);
}

void BitPumpPlain::skipBits(unsigned int nbits) {
  off += nbits;
  checkPos();
}

unsigned char BitPumpPlain::getByte() {
  guint v = *(guint*) & buffer[off>>3] >> (off & 7) & 0xff;
  off += 8;
  return v;
}

unsigned char BitPumpPlain::getByteSafe() {
  guint v = *(guint*) & buffer[off>>3] >> (off & 7) & 0xff;
  off += 8;
  checkPos();

  return v;
}

void BitPumpPlain::setAbsoluteOffset(unsigned int offset) {
  if (offset >= size)
    throw IOException("Offset set out of buffer");

  off = offset * 8;
}


BitPumpPlain::~BitPumpPlain(void) {
}

} // namespace RawSpeed
