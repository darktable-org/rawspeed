#include "StdAfx.h"
#include "RawDecoder.h"

RawDecoder::RawDecoder(FileMap* file) : mFile(file), mRaw(RawImage::create())
{
}

RawDecoder::~RawDecoder(void)
{

}
void RawDecoder::readUncompressedRaw(unsigned int offset, int max_offset, int* colorOrder) {
  _ASSERTE(mRaw->dim.Area());
  if (mRaw->bpp != 2)
    ThrowRDE("RawDecoder: Unsupported bit depth.");
  // Prepare local variables.
  const unsigned short* raw = (const unsigned short*)mFile->getData(offset);
  iPoint2D p(mRaw->cfa.getSize());

/*  


  if (p == iPoint2D(2,2)) {  // Special case, 50% faster, optimize further by avoiding double pointer lookup.
    int x_mod = p.x - 1;
    const unsigned int* src = (const unsigned int*)mFile->getData(offset);
    for (int y = 0; y < mRaw->dim.y; y++) {
      RawImagePlaneWriter** p_curr = &planes[(y & 1) * 2];     // We adjust offset into the CFA array now.
      for (int x = 0; x < mRaw->dim.x; x+=2) {
        unsigned int c = *src++;
        *(p_curr[0]->ptr++) = c&0xffff;
        *(p_curr[1]->ptr++) = c>>16;
      }
      p_curr[0]->nextLine();
      p_curr[1]->nextLine();
    }  
    return;
  }

  int x_mod = p.x - 1;
  const unsigned short* src = (const unsigned short*)mFile->getData(offset);  
  for (int y = 0; y < mRaw->dim.y; y++) {
    RawImagePlaneWriter** p_curr = &planes[(y % p.y) * p.x];     // We adjust offset into the CFA array now.
    for (int x = 0; x < mRaw->dim.x; x++) {
      *(p_curr[x & x_mod]->ptr++) = *src++;
    }
    for (int i = 0; i< p.x; i++) // Todo: Only increment each plane once.
      p_curr[i]->nextLine();
  }  
*/
}

