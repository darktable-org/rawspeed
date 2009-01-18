#include "StdAfx.h"
#include "TiffParser.h"

TiffParser::TiffParser(FileMap* inputData): mInput(inputData), mRootIFD(0)
{

}


TiffParser::~TiffParser(void)
{
  if (mRootIFD)
    delete mRootIFD;  
  mRootIFD = NULL;
}

#ifdef CHECKSIZE
#undef CHECKSIZE
#endif
#ifdef CHECKPTR
#undef CHECKPTR
#endif

#define CHECKSIZE(A) if (A >= mInput->getSize()) throw TiffParserException("Error reading TIFF structure. File Corrupt")
#define CHECKPTR(A) if ((int)A >= ((int)(mInput->data) + size))) throw TiffParserException("Error reading TIFF structure. File Corrupt")

void TiffParser::parseData() {
  const unsigned char* data = mInput->getData(0);
  if (mInput->getSize() < 16)
    throw TiffParserException("Not a TIFF file (size too small)");
  if (data[0] != 0x49 || data[1] != 0x49) {
    endian = big;
    if (data[0] != 0x4D || data[1] != 0x4D) 
      throw TiffParserException("Not a TIFF file (ID)");

    if (data[3] != 42) 
      throw TiffParserException("Not a TIFF file (magic 42)");
  } else {
    endian = little;
    if (data[2] != 42 && data[2] != 0x52) // ORF has 0x52 - Brillant!
      throw TiffParserException("Not a TIFF file (magic 42)");
  }

  if (endian == little)
    mRootIFD = new TiffIFD();
  else
    mRootIFD = new TiffIFDBE();

  guint nextIFD;
  data = mInput->getData(4);
  if (endian == little) {
    nextIFD = *(int*)data;
  } else {
    nextIFD = (unsigned int)data[0] << 24 | (unsigned int)data[1] << 16 | (unsigned int)data[2] << 8 | (unsigned int)data[3];
  }
  while (nextIFD) {
    CHECKSIZE(nextIFD);

    if (endian == little)
      mRootIFD->mSubIFD.push_back(new TiffIFD(mInput, nextIFD));
    else
      mRootIFD->mSubIFD.push_back(new TiffIFDBE(mInput, nextIFD));

    nextIFD = mRootIFD->mSubIFD.back()->getNextIFD();
  }
}

RawDecoder* TiffParser::getDecompressor() {
  vector<TiffIFD*> potentials;
  potentials = mRootIFD->getIFDsWithTag(DNGVERSION);

  if (!potentials.empty()) {  // We have a dng image entry
    TiffIFD *t = potentials[0];
    const unsigned char* c = t->getEntry(DNGVERSION)->getData();
    if (c[0] > 1)
      throw TiffParserException("DNG version too new.");
    if (c[1] > 2)
      throw TiffParserException("DNG version not supported.");
    return new DngDecoder(mRootIFD, mInput);
  }

  potentials = mRootIFD->getIFDsWithTag(MAKE);

  if (!potentials.empty()) {  // We have make entry
    for (vector<TiffIFD*>::iterator i = potentials.begin(); i != potentials.end(); ++i) {
      string make = (*i)->getEntry(MAKE)->getString();
      if (!make.compare("Canon")) {
        return new Cr2Decoder(mRootIFD,mInput);
      }
      if (!make.compare("NIKON CORPORATION")) {
        return new NefDecoder(mRootIFD,mInput);
      }
      if (!make.compare("OLYMPUS IMAGING CORP.  ")) {
        return new OrfDecoder(mRootIFD,mInput);
      }
      if (!make.compare("SONY ")) {
        return new ArwDecoder(mRootIFD,mInput);
      }
      if (!make.compare("PENTAX Corporation ")) {
        return new PefDecoder(mRootIFD,mInput);
      }
    }
  }
  throw TiffParserException("No decoder found. Sorry.");
  return NULL;
}
