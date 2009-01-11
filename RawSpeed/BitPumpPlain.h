#pragma once
#include "ByteStream.h"

// Note: Allocated buffer MUST be at least size+sizeof(guint) large.


class BitPumpPlain
{
public:
  BitPumpPlain(ByteStream *s);
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
	guint off;                  // Offset in bytes
  const guint size;            // This if the end of buffer.
	guint masks[31];
private:
};


