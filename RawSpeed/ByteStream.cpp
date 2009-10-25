#include "StdAfx.h"
#include "ByteStream.h"
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

ByteStream::ByteStream(const guchar* _buffer, guint _size) :
    buffer(_buffer), size(_size), off(0) {

}

ByteStream::ByteStream(const ByteStream *b) :
    buffer(b->buffer), size(b->size), off(b->off) {

}

ByteStream::~ByteStream(void) {

}

guint ByteStream::peekByte() {
  return buffer[off];
}

void ByteStream::skipBytes(guint nbytes) {
  off += nbytes;
  if (off > size)
    throw IOException("Skipped out of buffer");
}

guchar ByteStream::getByte() {
  if (off >= size)
    throw IOException("Out of buffer read");
  return buffer[off++];
}

gushort ByteStream::getShort() {
  if (off + 1 >= size)
    throw IOException("Out of buffer read");
  guint a = buffer[off++];
  guint b = buffer[off++];
  // !!! ENDIAN SWAP
  return (a << 8) | b;
}

gint ByteStream::getInt() {
  if (off + 4 >= size)
    throw IOException("Out of buffer read");
  return *(gint*)&buffer[off+=4];
}

void ByteStream::setAbsoluteOffset(guint offset) {
  if (offset >= size)
    throw IOException("Offset set out of buffer");
  off = offset;
}

void ByteStream::skipToMarker() {
  gint c = 0;
  while (!(buffer[off] == 0xFF && buffer[off+1] != 0)) {
    off++;
    c++;
    if (off >= size)
      throw IOException("No marker found inside rest of buffer");
  }
//  _RPT1(0,"Skipped %u bytes.\n", c);
}

} // namespace RawSpeed
