#pragma once
#include "ColorFilterArray.h"


class RawImage;
class RawImageData
{

public:
  virtual ~RawImageData(void);
  iPoint2D dim;
  guint bpp;      // Bytes per pixel.
  guint pitch;
  void RawImageData::createData();
  ColorFilterArray cfa;
  guchar* getData() {return data;}
  gboolean isCFA;
private:
  RawImageData(void);
  RawImageData(iPoint2D dim, guint bpp);
  guchar* data; 
  guint dataRefCount;
  friend class RawImage;
};


 class RawImage {
 public:
   static RawImage create();
   static RawImage create(iPoint2D dim, int bitsPerComponent);
   RawImageData* operator-> () { return p_; }
   RawImageData& operator* ()  { return *p_; }
   RawImage(RawImageData* p)    : p_(p) { ++p_->dataRefCount; }  // p must not be NULL
  ~RawImage()           { if (--p_->dataRefCount == 0) delete p_; }
   RawImage(const RawImage& p) : p_(p.p_) { ++p_->dataRefCount; }
   RawImage& operator= (const RawImage& p)
         { 
           RawImageData* const old = p_;
           p_ = p.p_;
           ++p_->dataRefCount;
           if (--old->dataRefCount == 0) delete old;
           return *this;
         }

 private:
   RawImageData* p_;    // p_ is never NULL
 };

inline RawImage RawImage::create()  { return new RawImageData(); }
inline RawImage RawImage::create(iPoint2D dim, int bitsPerComponent)
{ return new RawImageData(dim, bitsPerComponent); }
