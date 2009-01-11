#include "StdAfx.h"
#include "BitPumpPlain.h"

/*** Used for entropy encoded sections ***/

#define BITS_PER_LONG	(8*sizeof(guint))
#define MIN_GET_BITS  (BITS_PER_LONG-7)	   /* max value for long getBuffer */


BitPumpPlain::BitPumpPlain( ByteStream *s ):
  buffer(s->getData()), size(8*s->getRemainSize()), off(0)
  {
    for (int i = 0; i < 31; i++) {
      masks[i] = (1<<i)-1;
    }
  }

guint BitPumpPlain::getBit() {
  guint v = *(guint*)&buffer[off>>3] >> (off&7) & 1;
  off++;
  return v;
}

guint BitPumpPlain::getBits(guint nbits) {
  guint v = *(guint*)&buffer[off>>3] >> (off&7) & masks[nbits];
  off+=nbits;
  return v;
}

guint BitPumpPlain::peekBit() {
  return *(guint*)&buffer[off>>3] >> (off&7) & 1;
}

guint BitPumpPlain::peekBits(guint nbits) {
  return *(guint*)&buffer[off>>3] >> (off&7) & masks[nbits];
}

guint BitPumpPlain::peekByte() {
  return *(guint*)&buffer[off>>3] >> (off&7) & 0xff;
}

guint BitPumpPlain::getBitSafe() {
  if (off>size)
    throw IOException("Out of buffer read");
  return *(guint*)&buffer[off>>3] >> (off&7) & 1;
}

guint BitPumpPlain::getBitsSafe(unsigned int nbits) { 
  if (off>size)
    throw IOException("Out of buffer read");
  return *(guint*)&buffer[off>>3] >> (off&7) & masks[nbits];
}

void BitPumpPlain::skipBits(unsigned int nbits) {
  off+=nbits;
  if (off>size)
    throw IOException("Out of buffer read");  
}

unsigned char BitPumpPlain::getByte() {
  guint v = *(guint*)&buffer[off>>3] >> (off&7) & 0xff;
  off += 8;
  return v;
}

unsigned char BitPumpPlain::getByteSafe() {
  guint v = *(guint*)&buffer[off>>3] >> (off&7) & 0xff;
  off+=8;
  if (off>size)
    throw IOException("Out of buffer read");
  return v;
}

void BitPumpPlain::setAbsoluteOffset(unsigned int offset) {
  if (offset >= size) 
      throw IOException("Offset set out of buffer");
  off = offset*8;
}


BitPumpPlain::~BitPumpPlain(void)
{
}
