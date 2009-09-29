#include "StdAfx.h"
#include "BitPumpMSB.h"
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

/*** Used for entropy encoded sections ***/

#define BITS_PER_LONG (8*sizeof(guint))
#define MIN_GET_BITS  (BITS_PER_LONG-7)    /* max value for long getBuffer */


BitPumpMSB::BitPumpMSB(ByteStream *s):
    buffer(s->getData()), size(s->getRemainSize() + sizeof(guint)), mLeft(0), mCurr(0), off(0) {
  init();
}

BitPumpMSB::BitPumpMSB(const guchar* _buffer, guint _size) :
    buffer(_buffer), size(_size + sizeof(guint)), mLeft(0), mCurr(0), off(0) {
  init();
}

void __inline BitPumpMSB::init() {
  for (int i = 0; i < 31; i++) {
    masks[i] = (1 << i) - 1;
  }

  fill();
}

void BitPumpMSB::fill() {
  guchar c;

  while (mLeft < MIN_GET_BITS) {
    _ASSERTE(off < size);
    c = buffer[off++];
    mCurr = (mCurr << 8) | c;
    mLeft += 8;
  }
}

guint BitPumpMSB::getBit() {
  if (!mLeft) fill();

  return (mCurr >> (--mLeft)) & 1;
}

guint BitPumpMSB::getBits(guint nbits) {
  if (mLeft < nbits) {
    fill();
  }

  return ((mCurr >> (mLeft -= (nbits)))) & masks[nbits];
}

guint BitPumpMSB::peekBit() {
  if (!mLeft) fill();

  return (mCurr >> (mLeft - 1)) & 1;
}

guint BitPumpMSB::peekBits(guint nbits) {
  if (mLeft < nbits) {
    fill();
  }

  return ((mCurr >> (mLeft - nbits))) & masks[nbits];
}

guint BitPumpMSB::peekByte() {
  if (mLeft < 8) {
    fill();
  }

  if (off > size)
    throw IOException("Out of buffer read");

  return ((mCurr >> (mLeft - 8))) & 0xff;
}

guint BitPumpMSB::getBitSafe() {
  if (!mLeft) {
    fill();

    if (off > size)
      throw IOException("Out of buffer read");
  }

  return (mCurr >> (--mLeft)) & 1;
}

guint BitPumpMSB::getBitsSafe(unsigned int nbits) {
  if (nbits > MIN_GET_BITS)
    throw IOException("Too many bits requested");

  if (mLeft < nbits) {
    fill();

    if (off > size)
      throw IOException("Out of buffer read");
  }

  return ((mCurr >> (mLeft -= (nbits)))) & masks[nbits];
}

void BitPumpMSB::skipBits(unsigned int nbits) {
  if (mLeft < nbits) {
    fill();

    if (off > size)
      throw IOException("Out of buffer read");
  }

  mLeft -= nbits;
}

unsigned char BitPumpMSB::getByte() {
  if (mLeft < 8) {
    fill();
  }

  return ((mCurr >> (mLeft -= 8))) & 0xff;
}

unsigned char BitPumpMSB::getByteSafe() {
  if (mLeft < 8) {
    fill();

    if (off > size)
      throw IOException("Out of buffer read");
  }

  return ((mCurr >> (mLeft -= 8))) & 0xff;
}

void BitPumpMSB::setAbsoluteOffset(unsigned int offset) {
  if (offset >= size)
    throw IOException("Offset set out of buffer");

  mLeft = 0;

  mCurr = 0;

  off = offset;
}



BitPumpMSB::~BitPumpMSB(void) {
}

