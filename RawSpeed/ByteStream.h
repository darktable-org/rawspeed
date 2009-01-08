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
  guint getRemainSize() { return off-size;}
  const guchar* getData() {return &buffer[off];}
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