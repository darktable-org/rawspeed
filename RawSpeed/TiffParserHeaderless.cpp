#include "StdAfx.h"
#include "TiffParserHeaderless.h"

TiffParserHeaderless::TiffParserHeaderless(FileMap* input, Endianness _end) :
TiffParser(input)
{
  endian = _end;
}

TiffParserHeaderless::~TiffParserHeaderless(void)
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


void TiffParserHeaderless::parseData(guint firstIfdOffset) {
  const unsigned char* data = mInput->getData(0);
  if (mInput->getSize() < 12)
    throw TiffParserException("Not a TIFF file (size too small)");

  if (endian == little)
    mRootIFD = new TiffIFD();
  else
    mRootIFD = new TiffIFDBE();

  guint nextIFD = firstIfdOffset;
  do {
    CHECKSIZE(nextIFD);

    if (endian == little)
      mRootIFD->mSubIFD.push_back(new TiffIFD(mInput, nextIFD));
    else
      mRootIFD->mSubIFD.push_back(new TiffIFDBE(mInput, nextIFD));

    nextIFD = mRootIFD->mSubIFD.back()->getNextIFD();
  } while (nextIFD);
}
