#include "StdAfx.h"
#include "RawImage.h"
#include "RawDecompressor.h"  // For exceptions

RawImageData::RawImageData(void): 
dim(0,0), bpp(0), dataRefCount(0),data(0),isCFA(true)
{
}

RawImageData::RawImageData(iPoint2D _dim, guint _bpp) : 
dim(_dim), bpp(_bpp), dataRefCount(0),data(0) {
  createData();
}

RawImageData::~RawImageData(void)
{
  _ASSERTE(dataRefCount == 0);
  if (data)
	  _aligned_free(data);
  data = 0;
}


void RawImageData::createData() {
  pitch = (((dim.x*bpp) + 15)/16)*16;
  data = (guchar*)_aligned_malloc(pitch*dim.y,16);
}
