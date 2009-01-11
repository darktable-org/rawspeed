#include "StdAfx.h"
#include "ByteStream.h"


ByteStream::ByteStream( const guchar* _buffer, guint _size ) : 
buffer(_buffer), size(_size), off(0)
{

}

ByteStream::~ByteStream(void)
{

}

guint ByteStream::peekByte()
{
  return buffer[off];
}

void ByteStream::skipBytes( guint nbytes )
{
 off += nbytes;
 if (off>=size)
   throw IOException("Skipped out of buffer");
}

guchar ByteStream::getByte()
{
  if (off>=size)
    throw IOException("Out of buffer read");
  return buffer[off++];
}

gushort ByteStream::getShort()
{
  if (off+1>=size)
    throw IOException("Out of buffer read");
  guint a= buffer[off++];
  guint b = buffer[off++];
  // !!! ENDIAN SWAP
  return (a<<8)|b;
}

gint ByteStream::getInt() {
  if (off+4>=size)
    throw IOException("Out of buffer read");
  return *(gint*)&buffer[off+=4];
}

void ByteStream::setAbsoluteOffset( guint offset )
{
  if (offset >= size) 
    throw IOException("Offset set out of buffer");
  off = offset;
}

void ByteStream::skipToMarker()
{
  gint c=0;
  while (!(buffer[off] == 0xFF && buffer[off+1] != 0)) {
    off++;
    c++;
    if (off>=size)
      throw IOException("No marker found inside rest of buffer");
  }
  _RPT1(0,"Skipped %u bytes.\n", c);
}
