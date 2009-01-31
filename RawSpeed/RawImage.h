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
#include "pthread.h"
#include "ColorFilterArray.h"

class RawImage;
class RawImageData
{

public:
  virtual ~RawImageData(void);
  iPoint2D dim;
  guint bpp;      // Bytes per pixel.
  guint getCpp() const { return cpp; }
  void setCpp(guint val);
  guint pitch;
  void createData();
  guchar* getData();
  gboolean isCFA;
  ColorFilterArray cfa;
private:
  guint cpp;      // Components per pixel
  RawImageData(void);
  RawImageData(iPoint2D dim, guint bpp, guint cpp=1);
  guchar* data; 
  guint dataRefCount;
  friend class RawImage;
  pthread_mutex_t mymutex;
};


 class RawImage {
 public:
   static RawImage create();
   static RawImage create(iPoint2D dim, guint bytesPerComponent, guint componentsPerPixel = 1);
   RawImageData* operator-> ();
   RawImageData& operator* ();
   RawImage(RawImageData* p);  // p must not be NULL
  ~RawImage();
   RawImage(const RawImage& p);
   RawImage& operator= (const RawImage& p);

 private:
   RawImageData* p_;    // p_ is never NULL
 };

inline RawImage RawImage::create()  { return new RawImageData(); }
inline RawImage RawImage::create(iPoint2D dim, guint bytesPerPixel, guint componentsPerPixel)
{ return new RawImageData(dim, bytesPerPixel, componentsPerPixel); }
