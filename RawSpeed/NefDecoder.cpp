#include "StdAfx.h"
#include "NefDecoder.h"
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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

    http://www.klauspost.com
*/

NefDecoder::NefDecoder(TiffIFD *rootIFD, FileMap* file ) : 
RawDecoder(file), mRootIFD(rootIFD)
{

}

NefDecoder::~NefDecoder(void)
{
}

RawImage NefDecoder::decodeRaw()
{
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(CFAPATTERN);

  if (data.empty())
    ThrowRDE("NEF Decoder: No image data found");

  TiffIFD* raw = data[0];
  int compression = raw->getEntry(COMPRESSION)->getInt();

  data = mRootIFD->getIFDsWithTag(MODEL);

  if (data.empty())
    ThrowRDE("NEF Decoder: No model data found");

  TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);

  if (!data[0]->getEntry(MODEL)->getString().compare("NIKON D100 ")) {  /**Sigh**/
    if (!D100IsCompressed(offsets->getInt())) {
      DecodeD100Uncompressed();
      return mRaw;
    }
  }

  if (compression==1) {
    DecodeUncompressed();
    return mRaw;
  }

  if (offsets->count != 1) {
    ThrowRDE("NEF Decoder: Multiple Strips found: %u",offsets->count);
  }
  if (counts->count != offsets->count) {
    ThrowRDE("NEF Decoder: Byte count number does not match strip size: count:%u, strips:%u ",counts->count, offsets->count);
  }
  if (!mFile->isValid(offsets->getInt()+counts->getInt()))
    ThrowRDE("NEF Decoder: Invalid strip byte count. File probably truncated.");


  if (34713 != compression)
    ThrowRDE("NEF Decoder: Unsupported compression");

  guint width = raw->getEntry(IMAGEWIDTH)->getInt();
  guint height = raw->getEntry(IMAGELENGTH)->getInt();
  guint bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getInt();

  mRaw->dim = iPoint2D(width, height);
  mRaw->bpp = 2;
  mRaw->createData();

  data = mRootIFD->getIFDsWithTag(MAKERNOTE);
  if (data.empty())
    ThrowRDE("NEF Decoder: No EXIF data found");

  TiffIFD* exif = data[0];
  TiffEntry *makernoteEntry = exif->getEntry(MAKERNOTE);
  const guchar* makernote = makernoteEntry->getData();
  FileMap makermap((guchar*)&makernote[10], makernoteEntry->count-10);
  TiffParser makertiff(&makermap);
  makertiff.parseData();
  
  data = makertiff.RootIFD()->getIFDsWithTag((TiffTag)0x8c);

  if (data.empty())
    ThrowRDE("NEF Decoder: Decompression info tag not found");

  TiffEntry *meta;
  try {
    meta = data[0]->getEntry((TiffTag)0x96);
  } catch (TiffParserException) {
    meta = data[0]->getEntry((TiffTag)0x8c);  // Fall back
  }
 
  ByteStream metadata(meta->getData(), meta->count);
  
  NikonDecompressor decompressor(mFile,mRaw);
  decompressor.DecompressNikon(metadata, width, height, bitPerPixel,offsets->getInt(),counts->getInt());

  return mRaw;
}

/*
Figure out if a NEF file is compressed.  These fancy heuristics
are only needed for the D100, thanks to a bug in some cameras
that tags all images as "compressed".
*/
gboolean NefDecoder::D100IsCompressed(guint offset)
{
  const guchar *test = mFile->getData(offset);
  gint i;

  for (i=15; i < 256; i+=16)
    if (test[i]) return true;
  return false;
} 

void NefDecoder::DecodeUncompressed() {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(CFAPATTERN);
  TiffIFD* raw = data[0];
  guint nslices = raw->getEntry(STRIPOFFSETS)->count;
  const guint *offsets = raw->getEntry(STRIPOFFSETS)->getIntArray();
  const guint *counts = raw->getEntry(STRIPBYTECOUNTS)->getIntArray();
  guint yPerSlice = raw->getEntry(ROWSPERSTRIP)->getInt();
  guint width = raw->getEntry(IMAGEWIDTH)->getInt();
  guint height = raw->getEntry(IMAGELENGTH)->getInt();
  guint bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getInt();

  vector<NefSlice> slices;
  guint offY = 0;

  for (guint s = 0; s<nslices; s++) {
    NefSlice slice;
    slice.offset = offsets[s];
    slice.count = counts[s];
    if (offY+yPerSlice>height)
      slice.h = height-offY;
    else
      slice.h = yPerSlice;

    offY +=yPerSlice;

    if (mFile->isValid(slice.offset+slice.count)) // Only decode if size is valid
      slices.push_back(slice);
  }

  mRaw->dim = iPoint2D(width, offY);
  mRaw->bpp = 2;
  mRaw->createData();
  if (bitPerPixel == 14 && width*slices[0].h*2 == slices[0].count)
    bitPerPixel = 16; // D3

  offY = 0;
  for (guint i = 0; i< slices.size(); i++) {
    NefSlice slice = slices[i];
    ByteStream in(mFile->getData(slice.offset),slice.count);
    iPoint2D size(width,slice.h);
    iPoint2D pos(0,offY);
    readUncompressedRaw(in,size,pos,width*bitPerPixel/8,bitPerPixel,true);
    offY += slice.h;
  }
}

void NefDecoder::DecodeD100Uncompressed() {

  ThrowRDE("NEF DEcoder: D100 uncompressed not supported");
/*
  TiffIFD* raw = mRootIFD->getIFDsWithTag(CFAPATTERN)[0];

  guint nslices = raw->getEntry(STRIPOFFSETS)->count;
  guint offset = raw->getEntry(STRIPOFFSETS)->getInt();
  guint count = raw->getEntry(STRIPBYTECOUNTS)->getInt();
  if (!mFile->isValid(offset+count))
    ThrowRDE("DecodeD100Uncompressed: Truncated file");

  const guchar *in =  mFile->getData(offset);

  guint w = raw->getEntry(IMAGEWIDTH)->getInt();
  guint h = raw->getEntry(IMAGELENGTH)->getInt();

  mRaw->dim = iPoint2D(w, h);
  mRaw->bpp = 2;
  mRaw->createData();

  guchar* data = mRaw->getData();
  guint outPitch = mRaw->pitch;

  BitPumpMSB bits(mFile->getData(offset),count);
  for (guint y = 0; y < h; y++) {
    gushort* dest = (gushort*)&data[y*outPitch];
    for(guint x =0 ; x < w; x++) {
      guint b = bits.getBits(12);
      dest[x] = b;
      if ((x % 10) == 9)
        bits.skipBits(8);
    }
  }*/
}

void NefDecoder::checkSupport(CameraMetaData *meta) {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);
  if (data.empty())
    ThrowRDE("NEF Support check: Model name found");
  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();
  this->checkCameraSupported(meta, make, model, "");
}

void NefDecoder::decodeMetaData(CameraMetaData *meta)
{
  mRaw->cfa.setCFA(CFA_RED, CFA_GREEN, CFA_GREEN2, CFA_BLUE);

  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);

  if (data.empty())
    ThrowRDE("NEF Meta Decoder: Model name found");

  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();

  setMetaData(meta, make, model,"");

/*  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);

  if (data.empty())
    ThrowRDE("CR2 Decoder: Model name found");

  string model(data[0]->getEntry(MODEL)->getString());

  if (!model.compare("NIKON D2H")  ||
      !model.compare("NIKON D40X") ||
      !model.compare("NIKON D60") ||
      !model.compare("NIKON D90") ||
      !model.compare("NIKON D80") )
  {
    mRaw->cfa.setCFA(CFA_GREEN2, CFA_BLUE, CFA_RED, CFA_GREEN);   
  }

  if (!model.compare("NIKON D100 ") ||
    !model.compare("NIKON D200") )
  {
    mRaw->cfa.setCFA(CFA_GREEN2, CFA_RED, CFA_BLUE, CFA_GREEN);   
  }

  if (!model.compare("NIKON D1 ") ||
    !model.compare("NIKON D1H") ||
    !model.compare("NIKON D40")  ||
    !model.compare("NIKON D50") ||
    !model.compare("NIKON D70s") ||  
    !model.compare("NIKON D70") )
  {
    mRaw->cfa.setCFA(CFA_BLUE, CFA_GREEN, CFA_GREEN2, CFA_RED);
  }
*/
}