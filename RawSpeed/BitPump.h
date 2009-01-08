#pragma once
#include "ByteStream.h"

// Note: Allocated buffer MUST be at least size+sizeof(guint) large.


class BitPump
{
public:
  BitPump(ByteStream *s);
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
  guint getOffset() { return off-(mLeft>>3);}
  __inline guint getBitNoFill() {return (mCurr >> (--mLeft)) & 1;}
  __inline guint BitPump::peekByteNoFill() {return ((mCurr >> (mLeft-8))) & 0xff; }
  __inline guint BitPump::getBitsNoFill(guint nbits) {return ((mCurr >> (mLeft -= (nbits)))) & masks[nbits];}
  void fill();  // Fill the buffer with at least 24 bits


  virtual ~BitPump(void);
protected:
  const guchar* buffer;
	guint off;                  // Offset in bytes
  const guint size;            // This if the end of buffer.
	guint masks[31];
  guint mCurr;
  guint mLeft;
private:
};


