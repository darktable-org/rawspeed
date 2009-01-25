#pragma once
#include "ColorFilterArray.h"
#include "pthread.h"

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
  pthread_mutex_t mymutex;
};


 class RawImage {
 public:
   static RawImage create();
   static RawImage create(iPoint2D dim, int bitsPerComponent);
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
inline RawImage RawImage::create(iPoint2D dim, int bitsPerComponent)
{ return new RawImageData(dim, bitsPerComponent); }
