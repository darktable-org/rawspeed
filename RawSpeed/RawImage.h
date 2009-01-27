#pragma once
#include "pthread.h"

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
