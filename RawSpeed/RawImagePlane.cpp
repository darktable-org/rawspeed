#include "StdAfx.h"
#include "RawImagePlane.h"

RawImagePlane::RawImagePlane(void) : mScan(0), dim(0,0), color(CFA_COLOR_MIN), pitch(0),  bpp(2) 
{
  writer = new RawImagePlaneWriter(this);
}

RawImagePlane::RawImagePlane(iPoint2D _dim, CFAColor _color) : 
dim(_dim), color(_color), mScan(0), pitch(0), bpp(2) 
{
  writer = new RawImagePlaneWriter(this);
}

RawImagePlane::~RawImagePlane(void)
{
  delete writer;
  if (mScan)
    _aligned_free(mScan);
  mScan = 0;
}

void RawImagePlane::allocateScan() {
  _ASSERTE(dim.Area());
  pitch = ((dim.x*bpp+15)/16)*16;
  mScan = (unsigned short*)_aligned_malloc(pitch*dim.y,16);
}

RawImagePlaneWriter* RawImagePlane::getWriter() { 
  writer->reset();
  return writer; 
}

