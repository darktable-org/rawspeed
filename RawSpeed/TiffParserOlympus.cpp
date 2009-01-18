#include "StdAfx.h"
#include "TiffParserOlympus.h"

// More relaxed Tiff parser for olympus makernote

TiffParserOlympus::TiffParserOlympus(FileMap* input) :
TiffParser(input) {
}

TiffParserOlympus::~TiffParserOlympus(void)
{
}

#ifdef CHECKSIZE
#undef CHECKSIZE
#endif
#ifdef CHECKPTR
#undef CHECKPTR
#endif

#define CHECKSIZE(A) if (A >= mInput->getSize()) throw TiffParserException("Error reading TIFF structure. File Corrupt")
#define CHECKPTR(A) if ((int)A >= ((int)(mInput->data) + size))) throw TiffParserException("Error reading TIFF structure. File Corrupt")


void TiffParserOlympus::parseData() {
  const unsigned char* data = mInput->getData(0);
  if (mInput->getSize() < 16)
    throw TiffParserException("Not a TIFF file (size too small)");
  if (data[0] != 0x49 || data[1] != 0x49) {
    endian = big;
    if (data[0] != 0x4D || data[1] != 0x4D) 
      throw TiffParserException("Not a TIFF file (ID)");

  } else {
    endian = little;
  }

  if (endian == little)
    mRootIFD = new TiffIFD();
  else
    mRootIFD = new TiffIFDBE();

  guint nextIFD = 4;  // Skip Endian and magic
  do {
    CHECKSIZE(nextIFD);

    if (endian == little)
      mRootIFD->mSubIFD.push_back(new TiffIFD(mInput, nextIFD));
    else
      mRootIFD->mSubIFD.push_back(new TiffIFDBE(mInput, nextIFD));

    nextIFD = mRootIFD->mSubIFD.back()->getNextIFD();
  } while (nextIFD);
}
