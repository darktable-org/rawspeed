#pragma once
#include "ByteStream.h"

// Note: Allocated buffer MUST be at least size+sizeof(guint) large.


class BitPumpMSB
{
public:
  BitPumpMSB(ByteStream *s);
  BitPumpMSB(const guchar* _buffer, guint _size );
	guint getBits(guint nbits);
	guint getBit();
	guint getBitsSafe(guint nbits);
	guint getBitSafe();
	guint peekBits(guint nbits);
	guint peekBit();
  guint peekByte();
  void skipBits(guint nbits);
  __inline void checkPos()  { if (off>size) throw IOException("Out of buffer read");};        // Check if we have a valid position
	guchar getByte();
	guchar getByteSafe();
	void setAbsoluteOffset(guint offset);     // Set offset in bytes
  guint getOffset() { return off-(mLeft>>3);}
  __inline guint getBitNoFill() {return (mCurr >> (--mLeft)) & 1;}
  __inline guint peekByteNoFill() {return ((mCurr >> (mLeft-8))) & 0xff; }
  __inline guint getBitsNoFill(guint nbits) {return ((mCurr >> (mLeft -= (nbits)))) & masks[nbits];}
  __inline guint peekBitsNoFill(guint nbits) {return ((mCurr >> (mLeft-nbits))) &masks[nbits]; }
  void fill();  // Fill the buffer with at least 24 bits


  virtual ~BitPumpMSB(void);
protected:
  void __inline init();
  const guchar* buffer;
	guint off;                  // Offset in bytes
  const guint size;            // This if the end of buffer.
	guint masks[31];
  guint mCurr;
  guint mLeft;
private:
};


