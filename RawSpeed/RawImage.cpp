#include "StdAfx.h"
#include "RawImage.h"
#include "RawDecoder.h"  // For exceptions
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

RawImageData::RawImageData(void): 
dim(0,0), bpp(0), isCFA(true), dataRefCount(0),data(0), cpp(1)
{
  pthread_mutex_init(&mymutex, NULL);
}

RawImageData::RawImageData(iPoint2D _dim, guint _bpc, guint cpp) : 
dim(_dim), bpp(_bpc), dataRefCount(0),data(0) {
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

void RawImageData::setCpp( guint val )
{
  if (data)
    ThrowRDE("RawImageData: Attempted to set Components per pixel after data allocation");
  bpp /= cpp;
  cpp = val;
  bpp *= val;
}

guchar* RawImageData::getData()
{
  if (!data)
    ThrowRDE("RawImageData::getData - Data not yet allocated.");
  return &data[mOffset.y*pitch+mOffset.x*bpp];
}

guchar* RawImageData::getData( guint x, guint y )
{
  x+= mOffset.x;
  y+= mOffset.y;

  if (!data)
    ThrowRDE("RawImageData::getData - Data not yet allocated.");
  if (x>=dim.x)
    ThrowRDE("RawImageData::getData - X Position outside image requested.");
  if (y>=dim.y)
    ThrowRDE("RawImageData::getData - Y Position outside image requested.");

  return &data[y*pitch+x*bpp];
}

void RawImageData::subFrame( iPoint2D offset, iPoint2D new_size )
{
  if (!new_size.isThisInside(dim+offset+mOffset))
    ThrowRDE("RawImageData::subFrame - Attempted to create new subframe larger than original size.");

  mOffset += offset;
  dim = new_size;
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
