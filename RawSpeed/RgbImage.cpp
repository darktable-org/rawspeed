#include "StdAfx.h"
#include "RgbImage.h"

RgbImage::RgbImage(int _w, int _h, int _bpp) : w(_w), h(_h), bpp(_bpp), owned(true) {
  pitch = ((w*bpp+15)/16)*16;
  data = (unsigned char*)_aligned_malloc(pitch*h,16);
}

RgbImage::RgbImage(int _w, int _h, int _bpp, int _pitch, unsigned char* _data) : 
w(_w), h(_h), bpp(_bpp), pitch(_pitch), data(_data), owned(false) {

}


RgbImage::~RgbImage(void)
{
  if (data && owned) {
    _aligned_free(data);
  }
}
