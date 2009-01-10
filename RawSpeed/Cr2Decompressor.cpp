#include "StdAfx.h"
#include "Cr2Decompressor.h"

Cr2Decompressor::Cr2Decompressor(TiffIFD *rootIFD, FileMap* file) :
 RawDecompressor(file), mRootIFD(rootIFD) 
{

}

Cr2Decompressor::~Cr2Decompressor(void)
{
}

RawImage Cr2Decompressor::decodeRaw()
{
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag((TiffTag)0xc5d8);

  if (data.empty())
    ThrowRDE("CR2 Decoder: No image data found");

  TiffIFD* raw = data[0];
  mRaw = RawImage::create();
  mRaw->isCFA = true;
  vector<Cr2Slice> slices;
  int completeW = 0;

  try {
    TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
    TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);
    // Iterate through all slices
    for (int s = 0; s<offsets->count; s++) {
      Cr2Slice slice;
      slice.offset = offsets[0].getInt();
      slice.count = counts[0].getInt();
      SOFInfo sof;
      LJpegDecompressor l(mFile,mRaw);
      l.getSOF(&sof,slice.offset,slice.count);
      slice.w = sof.w*sof.cps;
      slice.h = sof.h;
      if (!slices.empty())
        if (slices[0].h != slice.h)
          ThrowRDE("CR2 Decoder: Slice height does not match.");

      slices.push_back(slice);
      completeW += slice.w;
    }
  } catch (TiffParserException) {
    ThrowRDE("CR2 Decoder: Unsupported format.");
  }
  
  if (slices.empty()) {
    ThrowRDE("CR2 Decoder: No Slices found.");
  }

  mRaw->bpp = 2;
  mRaw->dim = iPoint2D(completeW, slices[0].h);
  mRaw->createData();

  guint offX = 0;

  vector<int> s_width;
  if (raw->hasEntry(CANONCR2SLICE)) {
    const gushort *ss = raw->getEntry(CANONCR2SLICE)->getShortArray();
    for (int i = 0; i < ss[0]; i++) {
      s_width.push_back(ss[1]);
    }
    s_width.push_back(ss[2]);
  } else {
    s_width.push_back(completeW);
  }

  for (int i = 0; i < slices.size(); i++ ) {  // This loop is obvious for threading, as slices are independent
    Cr2Slice slice = slices[i];
    try {
      LJpegDecompressor l(mFile,mRaw);
      l.addSlices(s_width);
      l.startDecoder(slice.offset, slice.count, offX, 0);
    } catch (RawDecompressorException* e) { 
      // These may just be single slice error - store the error and move on
      errors.push_back(_strdup(e->what()));
    }
    offX += slice.w;
  }

  return mRaw;
}