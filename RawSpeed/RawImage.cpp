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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

    http://www.klauspost.com
*/
namespace RawSpeed {

RawImageData::RawImageData(void):
    dim(0, 0), bpp(0), isCFA(true),
    blackLevel(-1), whitePoint(65536),
    dataRefCount(0), data(0), cpp(1) {
  pthread_mutex_init(&mymutex, NULL);
  subsampling.x = subsampling.y = 1;
}

RawImageData::RawImageData(iPoint2D _dim, guint _bpc, guint _cpp) :
    dim(_dim), bpp(_bpc),
    blackLevel(-1), whitePoint(65536),
    dataRefCount(0), data(0), cpp(cpp) {
  subsampling.x = subsampling.y = 1;
  createData();
  pthread_mutex_init(&mymutex, NULL);
}

RawImageData::~RawImageData(void) {
  _ASSERTE(dataRefCount == 0);
  if (data)
    _aligned_free(data);
  data = 0;
  mOffset = iPoint2D(0, 0);
  pthread_mutex_destroy(&mymutex);
}


void RawImageData::createData() {
  if (dim.x > 65535 || dim.y > 65535)
    ThrowRDE("RawImageData: Dimensions too large for allocation.");
  if (data)
    ThrowRDE("RawImageData: Duplicate data allocation in createData.");
  pitch = (((dim.x * bpp) + 15) / 16) * 16;
  data = (guchar*)_aligned_malloc(pitch * dim.y, 16);
  if (!data)
    ThrowRDE("RawImageData::createData: Memory Allocation failed.");
}

void RawImageData::destroyData() {
  if (data)
    _aligned_free(data);
  data = 0;
}

void RawImageData::setCpp(guint val) {
  if (data)
    ThrowRDE("RawImageData: Attempted to set Components per pixel after data allocation");
  bpp /= cpp;
  cpp = val;
  bpp *= val;
}

guchar* RawImageData::getData() {
  if (!data)
    ThrowRDE("RawImageData::getData - Data not yet allocated.");
  return &data[mOffset.y*pitch+mOffset.x*bpp];
}

guchar* RawImageData::getData(guint x, guint y) {
  if ((int)x >= dim.x)
    ThrowRDE("RawImageData::getData - X Position outside image requested.");
  if ((int)y >= dim.y) {
    ThrowRDE("RawImageData::getData - Y Position outside image requested.");
  }

  x += mOffset.x;
  y += mOffset.y;

  if (!data)
    ThrowRDE("RawImageData::getData - Data not yet allocated.");

  return &data[y*pitch+x*bpp];
}

void RawImageData::subFrame(iPoint2D offset, iPoint2D new_size) {
  if (!new_size.isThisInside(dim - offset)) {
    printf("WARNING: RawImageData::subFrame - Attempted to create new subframe larger than original size. Crop skipped.\n");
    return;
  }

  mOffset += offset;
  dim = new_size;
}

void RawImageData::scaleBlackWhite() {
  gint gw = (dim.x - 20) * cpp;
  if (blackLevel < 0 || whitePoint == 65536) {  // Estimate
    int b = 65536;
    int m = 0;
    for (int row = 10;row < (dim.y - 10);row++) {
      gushort *pixel = (gushort*)getData(10, row);
      for (int col = 10 ; col < gw ; col++) {
        b = MIN(*pixel, b);
        m = MAX(*pixel, m);
        pixel++;
      }
    }
    if (blackLevel < 0)
      blackLevel = b;
    if (whitePoint == 65536)
      whitePoint = m;
    printf("Estimated black:%d, Estimated white: %d\n", blackLevel, whitePoint);
  }
  float f = 65535.0f / (float)(whitePoint - blackLevel);
  scaleValues(f);
}

#if _MSC_VER > 1399

void RawImageData::scaleValues(float f) {
  int info[4];
  __cpuid(info, 1);

  // Check SSE2
  if (f >= 0.0f && info[3]&(1 << 26)) {

    __m128i ssescale;
    __m128i ssesub;
    __m128i sseround;
    __m128i ssesub2;
    __m128i ssesign;
    guint gw = pitch / 16;
    guint i = (int)(1024.0f * f);  // 10 bit fraction
    i |= i << 16;
    guint b = blackLevel | (blackLevel << 16);

    ssescale = _mm_set_epi32(i, i, i, i);
    ssesub = _mm_set_epi32(b, b, b, b);
    sseround = _mm_set_epi32(512, 512, 512, 512);
    ssesub2 = _mm_set_epi32(32768, 32768, 32768, 32768);
    ssesign = _mm_set_epi32(0x80008000, 0x80008000, 0x80008000, 0x80008000);

    for (int y = 0; y < dim.y; y++) {
      __m128i* pixel = (__m128i*) & data[(mOffset.y+y)*pitch];
      for (guint x = 0 ; x < gw; x++) {
        __m128i pix_high;
        __m128i temp;
        __m128i pix_low = _mm_load_si128(pixel);
        // Subtract black
        pix_low = _mm_subs_epu16(pix_low, ssesub);
        // Multiply the two unsigned shorts and combine it to 32 bit result
        pix_high = _mm_mulhi_epu16(pix_low, ssescale);
        temp = _mm_mullo_epi16(pix_low, ssescale);
        pix_low = _mm_unpacklo_epi16(temp, pix_high);
        pix_high = _mm_unpackhi_epi16(temp, pix_high);
        // Add rounder
        pix_low = _mm_add_epi32(pix_low, sseround);
        pix_high = _mm_add_epi32(pix_high, sseround);
        // Shift down
        pix_low = _mm_srai_epi32(pix_low, 10);
        pix_high = _mm_srai_epi32(pix_high, 10);
        // Subtract to avoid clipping
        pix_low = _mm_sub_epi32(pix_low, ssesub2);
        pix_high = _mm_sub_epi32(pix_high, ssesub2);
        // Pack
        pix_low = _mm_packs_epi32(pix_low, pix_high);
        // Shift sign off
        pix_low = _mm_xor_si128(pix_low, ssesign);
        _mm_store_si128(pixel, pix_low);
        pixel++;
      }
    }
  } else {
    // Not SSE2
    gint gw = dim.x * cpp;
    int scale = (int)(16384.0f * f);  // 14 bit fraction
    for (int y = 0; y < dim.y; y++) {
      gushort *pixel = (gushort*)getData(0, y);
      for (int x = 0 ; x < gw; x++) {
        pixel[x] = clampbits(((pixel[x] - blackLevel) * scale + 8192) >> 14, 16);
      }
    }
  }
}

#else

void RawImageData::scaleValues(float f) {
#if 0
  //TODO: Check for SSE2 on 32 bit systems and use it there
  guint temp[20];

  guint i = (int)(1024.0f * f);  // 10 bit fraction
  i |= i << 16;
  guint b = blackLevel | (blackLevel << 16);

  for (int j = 0; j < 4; j++) {
    temp[j] = b;
    temp[j+4] = i;
    temp[j+8] = 512;
    temp[j+12] = 32768;
    temp[j+16] = 0x80008000;
  }

  asm volatile
  (
    "movdqu 0(%0), %%xmm7\n"     // Subtraction
    "movdqu 16(%0), %%xmm6\n"    // Multiplication factor
    "movdqu 32(%0), %%xmm5\n"    // Fraction
    "movdqu 48(%0), %%xmm4\n"    // Sub 32768
    "movdqu 64(%0), %%xmm3\n"    // Sign shift
  : // no output registers
  : "r"(temp)
        : //  %0
      );

  for (int y = 0; y < dim.y; y++) {
    guchar* pixel = (guchar*) & data[(mOffset.y+y)*pitch];
    guint gw = pitch >> 4;
    for (guint x  = 0; x < gw ; x++) {
      asm volatile(
        "next_pixel:\n"
        "movaps 0(%0), %%xmm0\n"
        "psubusw %%xmm7, %%xmm0\n"  // Subtract black
        "movaps %%xmm0, %%xmm1\n"
        "pmullw %%xmm6, %%xmm0\n"
        "pmulhuw %%xmm6, %%xmm1\n"
        "movaps %%xmm0, %%xmm2\n"
        "punpcklwd %%xmm1, %%xmm0\n" // First 4 result
        "punpckhwd %%xmm1, %%xmm2\n" // Last 4 result
        "paddd %%xmm5, %%xmm0\n"      // Add fraction
        "paddd %%xmm5, %%xmm2\n"
        "psrad $10, %%xmm0\n"
        "psrad $10, %%xmm2\n"
        "psubd %%xmm4, %%xmm0\n"      // Avoid saturation
        "psubd %%xmm4, %%xmm2\n"
        "packssdw %%xmm2, %%xmm0\n"
        "pxor %%xmm3, %%xmm0\n"       // Shift sign
        "movaps %%xmm0, 0(%0)\n"

        "add $16, %0\n"
      : // no output registers
      : "r"(pixel)
            :  // %0
          );
    }
  }

#else

gint gw = dim.x * cpp;
int scale = (int)(16384.0f * f);  // 14 bit fraction
for (int y = 0; y < dim.y; y++) {
  gushort *pixel = (gushort*)getData(0, y);
  for (int x = 0 ; x < gw; x++) {
    pixel[x] = clampbits(((pixel[x] - blackLevel) * scale + 8192) >> 14, 16);
  }
}

#endif

}

#endif


RawImage::RawImage(RawImageData* p) : p_(p) {
  pthread_mutex_lock(&p_->mymutex);
  ++p_->dataRefCount;
  pthread_mutex_unlock(&p_->mymutex);
}

RawImage::RawImage(const RawImage& p) : p_(p.p_) {
  pthread_mutex_lock(&p_->mymutex);
  ++p_->dataRefCount;
  pthread_mutex_unlock(&p_->mymutex);
}

RawImage::~RawImage() {
  pthread_mutex_lock(&p_->mymutex);
  if (--p_->dataRefCount == 0) {
    pthread_mutex_unlock(&p_->mymutex);
    delete p_;
    return;
  }
  pthread_mutex_unlock(&p_->mymutex);
}

RawImageData* RawImage::operator->() {
  return p_;
}

RawImageData& RawImage::operator*() {
  return *p_;
}

RawImage& RawImage::operator=(const RawImage & p) {
  RawImageData* const old = p_;
  p_ = p.p_;
  ++p_->dataRefCount;
  if (--old->dataRefCount == 0) delete old;
  return *this;
}

} // namespace RawSpeed
