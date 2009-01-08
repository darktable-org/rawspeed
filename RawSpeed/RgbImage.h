#pragma once

class RgbImage
{
public:
  RgbImage(void) : w(0), h(0), pitch(0), bpp(0), data(0), owned(false) {}
  RgbImage(RgbImage &i) : w(i.w), h(i.h), pitch(i.pitch), bpp(i.pitch), data(i.data), owned(false) {}
  RgbImage(int w, int h, int bpp);
  RgbImage(int w, int h, int bpp, int pitch, unsigned char* data);

  virtual ~RgbImage(void);
  const bool owned;
  const int w,h;
  const int bpp;
  int pitch;
  unsigned char* data;
};
