#include "StdAfx.h"
#include "BitPump.h"

/*** Used for entropy encoded sections ***/

#define BITS_PER_LONG	(8*sizeof(guint))
#define MIN_GET_BITS  (BITS_PER_LONG-7)	   /* max value for long getBuffer */


BitPump::BitPump( ByteStream *s ):
  buffer(s->getData()), size(s->getRemainSize()+sizeof(guint)), mLeft(0),mCurr(0), off(0)
  {
    for (int i = 0; i < 31; i++) {
      masks[i] = (1<<i)-1;
    }
    fill();
  }

  void __inline BitPump::fill() {
    guchar c, c2;
    while (mLeft < MIN_GET_BITS) {
      _ASSERTE(off<size);
      c = buffer[off++];

      /*
      * If it's 0xFF, check and discard stuffed zero byte
      */
      if (c == 0xFF) {
        c2 = buffer[off];

        if (c2 == 0) { // Increment, if not a stuffed ff
          off++;
        }  // Otherwise just keep going
      }
      /*
      * OK, load c into mCurr
      */
      mCurr = (mCurr << 8) | c;
      mLeft += 8;
    }
  }

guint BitPump::getBit() {
  if (!mLeft) fill();
  return (mCurr >> (--mLeft)) & 1;
}

guint BitPump::getBits(guint nbits) {
  if (mLeft < nbits) {
    fill();
  }
  return ((mCurr >> (mLeft -= (nbits)))) & masks[nbits];
}

guint BitPump::peekBit() {
  if (!mLeft) fill();
  return (mCurr >> (mLeft-1)) & 1;
}

guint BitPump::peekBits(guint nbits) {
  if (mLeft < nbits) {
    fill();
  }
  return ((mCurr >> (mLeft-nbits))) & masks[nbits];
}

guint BitPump::peekByte() {
  if (mLeft < 8) {
    fill();
  }
  if (off>size)
    throw IOException("Out of buffer read");
  return ((mCurr >> (mLeft-8))) & 0xff;
}

guint BitPump::getBitSafe() {
  if (!mLeft) {
    fill();
    if (off>size)
      throw IOException("Out of buffer read");
  }
  return (mCurr >> (--mLeft)) & 1;
}

guint BitPump::getBitsSafe(unsigned int nbits) { 
  if (nbits>MIN_GET_BITS)
    throw IOException("Too many bits requested");
  if (mLeft < nbits) {
    fill();
    if (off>size)
      throw IOException("Out of buffer read");
  }
  return ((mCurr >> (mLeft -= (nbits)))) & masks[nbits];
}

void BitPump::skipBits(unsigned int nbits) {
  if (mLeft < nbits) {
    fill();
    if (off>size)
      throw IOException("Out of buffer read");
  }
  mLeft -= nbits;
}

unsigned char BitPump::getByte() {
  if (mLeft < 8) {
    fill();
  }
  return ((mCurr >> (mLeft -= 8))) & 0xff;
}

unsigned char BitPump::getByteSafe() {
  if (mLeft < 8) {
    fill();
    if (off>size)
      throw IOException("Out of buffer read");
  }
  return ((mCurr >> (mLeft -= 8))) & 0xff;
}

void BitPump::setAbsoluteOffset(unsigned int offset) {
  if (offset >= size) 
      throw IOException("Offset set out of buffer");
  mLeft = 0;
  off = offset;
}


BitPump::~BitPump(void)
{
}
