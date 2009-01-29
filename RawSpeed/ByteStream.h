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

class ByteStream
{
public:
  ByteStream(const guchar* _buffer, guint _size);
  ~ByteStream(void);
  guint peekByte();
  gushort getShort();
  void skipBytes(guint nbytes);
  guchar getByte();
  void setAbsoluteOffset(guint offset);
  void skipToMarker();
  guint getRemainSize() { return size-off;}
  const guchar* getData() {return &buffer[off];}
  gint getInt();
private:
  const guchar* buffer;
  guint off;                  // Offset in bytes (this is next byte to deliver)
  const guint size;            // This if the end of buffer.

};

class IOException : public std::runtime_error
{
public:
  IOException(const string _msg) : runtime_error(_msg) {
    _RPT1(0, "IO Exception: %s\n", _msg.c_str());
  }
};