#pragma once

#ifndef __unix__
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
#define MIN(a,b) min(a,b)
#define MAX(a,b) max(a,b)
#else
#include <rawstudio.h>
#include <exception>
#include <string.h>
#include <assert.h>
#define BYTE unsigned char
#define _ASSERTE(a)
#include <stdexcept>
#define _RPT0(a,b) 
#define _RPT1(a,b,c) 
#define _RPT2(a,b,c,d) 
#define _RPT3(a,b,c,d,e) 
#define _RPT4(a,b,c,d,e,f) 
#define __inline inline
#define _strdup(a) strdup(a)
#define _aligned_malloc(a, alignment) malloc(a)
#define _aligned_free(a) do { free(a); } while (0)
#define MIN(a, b)  lmin(a,b)
#define MAX(a, b)  lmin(a,b)
typedef char* LPCWSTR;
#endif // __unix__


inline void BitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) {
  if (height == 1 || (dst_pitch == src_pitch && src_pitch == row_size)) {
    memcpy(dstp, srcp, row_size*height);
    return;
  }
  for (int y=height; y>0; --y) {
    memcpy(dstp, srcp, row_size);
    dstp += dst_pitch;
    srcp += src_pitch;
  }
}

inline int lmin(int p0, int p1) {
  return p1 + ((p0 - p1) & ((p0 - p1) >> 31));
}
inline int lmax(int p0, int p1) {
  return p0 - ((p0 - p1) & ((p0 - p1) >> 31));
}
#define CLAMPBITS(x,n) { guint32 _y_temp; if( _y_temp=x>>n ) x = ~_y_temp >> (32-n);}



/* -------------------------------------------------------------------- */
#ifndef __unix__

typedef   bool          gboolean;
typedef   void*         gpointer;
typedef   const void*   gconstpointer;
typedef   char          gchar;
typedef   unsigned char guchar;

typedef  int           gint;
typedef  unsigned int  guint;
typedef  short           gshort;
typedef  unsigned short           gushort;
typedef  long           glong;
typedef  unsigned long           gulong;

typedef  char gint8;
typedef  unsigned char           guint8;
typedef  short           gint16;
typedef  unsigned short           guint16;
typedef  int           gint32;
typedef  unsigned int           guint32;

typedef  long long       gint64;
typedef  unsigned long long          guint64;

typedef float            gfloat;
typedef double            gdouble;

typedef unsigned int            gsize;
typedef signed int            gssize;
typedef gint64            goffset;
#endif // __unix__

