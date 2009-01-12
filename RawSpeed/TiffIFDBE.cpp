#include "StdAfx.h"
#include "TiffIFDBE.h"
#include "TiffEntryBE.h"

TiffIFDBE::TiffIFDBE() {
}

TiffIFDBE::TiffIFDBE(FileMap* f, guint offset)
{
  int size = f->getSize();
  int entries;

   const unsigned char* data = f->getData(offset);
   entries = (unsigned short)data[0] << 8 | (unsigned short)data[1];    // Directory entries in this IFD

  CHECKSIZE(offset+2+entries*4);
  for (int i = 0; i < entries; i++) {
    TiffEntryBE *t = new TiffEntryBE(f, offset+2+i*12);

    if (t->tag == 330) {   // subIFD tag
      const unsigned int* sub_offsets = t->getIntArray();
      for (int j = 0; j < t->count; j++ ) {
        mSubIFD.push_back(new TiffIFDBE(f, sub_offsets[j]));
      }
    } else {  // Store as entry
      mEntry[t->tag] = t;
    }
  }
  data = f->getDataWrt(offset+2+entries*12);
  nextIFD = (unsigned int)data[0] << 24 | (unsigned int)data[1] << 16 | (unsigned int)data[2] << 8 | (unsigned int)data[3];
}


TiffIFDBE::~TiffIFDBE(void)
{
}
