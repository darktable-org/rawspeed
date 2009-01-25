#include "StdAfx.h"
#include "RawImage.h"
#include "RawDecoder.h"  // For exceptions

RawImageData::RawImageData(void): 
dim(0,0), bpp(0), dataRefCount(0),data(0),isCFA(true)
{
  pthread_mutex_init(&mymutex, NULL);
}

RawImageData::RawImageData(iPoint2D _dim, guint _bpp) : 
dim(_dim), bpp(_bpp), dataRefCount(0),data(0) {
  createData();
  pthread_mutex_init(&mymutex, NULL);
}

RawImageData::~RawImageData(void)
{
  _ASSERTE(dataRefCount == 0);
  if (data)
	  _aligned_free(data);
  data = 0;
  pthread_mutex_destroy(&mymutex);
}


void RawImageData::createData() {  
  pitch = (((dim.x*bpp) + 15)/16)*16;
  data = (guchar*)_aligned_malloc(pitch*dim.y,16);
  if (!data)
    ThrowRDE("RawImageData::createData: Memory Allocation failed.");
}

RawImage::RawImage( RawImageData* p ) : p_(p)
{
  pthread_mutex_lock(&p_->mymutex);
  ++p_->dataRefCount;
  pthread_mutex_unlock(&p_->mymutex);
}

RawImage::RawImage( const RawImage& p ) : p_(p.p_)
{
  pthread_mutex_lock(&p_->mymutex);
  ++p_->dataRefCount;
  pthread_mutex_unlock(&p_->mymutex);
}
RawImage::~RawImage()
{
  pthread_mutex_lock(&p_->mymutex);
  if (--p_->dataRefCount == 0) {
    pthread_mutex_unlock(&p_->mymutex);
    delete p_;
    return;
  }
  pthread_mutex_unlock(&p_->mymutex);
}

RawImageData* RawImage::operator->()
{
  return p_;
}

RawImageData& RawImage::operator*()
{
  return *p_;
}

RawImage& RawImage::operator=( const RawImage& p )
{
  RawImageData* const old = p_;
  p_ = p.p_;
  ++p_->dataRefCount;
  if (--old->dataRefCount == 0) delete old;
  return *this;
}
