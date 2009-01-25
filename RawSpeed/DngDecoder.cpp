#include "StdAfx.h"
#include "DngDecoder.h"

DngDecoder::DngDecoder(TiffIFD *rootIFD, FileMap* file) : RawDecoder(file), mRootIFD(rootIFD)
{
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(DNGVERSION);
  const unsigned char* v = data[0]->getEntry(DNGVERSION)->getData();

  if (v[0] != 1)
    ThrowRDE("Not a supported DNG image format: v%u.%u.%u.%u",(int)v[0],(int)v[1],(int)v[2],(int)v[3]);
  if (v[1] > 2)
    ThrowRDE("Not a supported DNG image format: v%u.%u.%u.%u",(int)v[0],(int)v[1],(int)v[2],(int)v[3]);

  if ((v[0] <= 1) && (v[1] < 1) ) // Prior to v1.1.xxx  fix LJPEG encoding bug
    mFixLjpeg = true;
  else
    mFixLjpeg = false;
}

DngDecoder::~DngDecoder(void)
{
}

RawImage DngDecoder::decodeRaw() {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(COMPRESSION);

  if (data.empty())
    ThrowRDE("DNG Decoder: No image data found");

  // Erase the ones not with JPEG compression
  for (vector<TiffIFD*>::iterator i = data.begin(); i != data.end(); ) {
    int compression = (*i)->getEntry(COMPRESSION)->getShort();
    bool isSubsampled = false;
    try {
      isSubsampled = (*i)->getEntry(NEWSUBFILETYPE)->getInt()&1; // bit 0 is on if image is subsampled
    } catch (TiffParserException) {}
    if ((compression != 7 && compression != 1) || isSubsampled) {  // Erase if subsampled, or not JPEG or uncompressed
      i = data.erase(i);
    } else {
      i++;
    }
  }

  if (data.empty())
    ThrowRDE("DNG Decoder: No RAW chunks found");

  if (data.size()>1) {
    _RPT0(0,"Multiple RAW chunks found - using first only!");
  }

  TiffIFD* raw = data[0];
  mRaw = RawImage::create();
  mRaw->isCFA = (raw->getEntry(PHOTOMETRICINTERPRETATION)->getShort() == 32803);

  if (mRaw->isCFA) 
    _RPT0(0,"This is a CFA image\n");
  else 
    _RPT0(0,"This is NOT a CFA image\n");

  try {
    mRaw->dim.x = raw->getEntry(IMAGEWIDTH)->getInt();
    mRaw->dim.y = raw->getEntry(IMAGELENGTH)->getInt();
    mRaw->bpp = raw->getEntry(BITSPERSAMPLE)->getShort()/8;    
  } catch (TiffParserException) {
    ThrowRDE("DNG Decoder: Could not read basic image information.");
  }

  try {

    int compression = raw->getEntry(COMPRESSION)->getShort();
/*    if (mRaw->isCFA) {
      const unsigned short* pDim = raw->getEntry(CFAREPEATPATTERNDIM)->getShortArray(); // Get the size
      const unsigned char* cPat = raw->getEntry(CFAPATTERN)->getData();                 // Does NOT contain dimensions as some documents state
      const unsigned char* cPlaneOrder = raw->getEntry(CFAPLANECOLOR)->getData();       // Map from the order in the image, to the position in the CFA

      int patternSize = pDim[0]*pDim[1];
      if (patternSize != raw->getEntry(CFAPATTERN)->count)
        ThrowRDE("DNG Decoder: CFA pattern dimension and pattern count does not match: %d.");
      iPoint2D cfaSize(pDim[1],pDim[0]);
      mRaw->cfa = ColorFilterArray(cfaSize);
      for (int y = 0; y < cfaSize.y; y++) {
        for (int x = 0; x < cfaSize.x; x++) {
          mRaw->cfa.setColorAt(iPoint2D(x,y),(CFAColor)cPat[x+y*cfaSize.x]);
        }
      }
    }
*/
    
    // Now load the image
    if (compression == 1) {  // Uncompressed.
      try {
        TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
        TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);
        if (offsets->count != 1) {
          ThrowRDE("DNG Decoder: Multiple Strips found: %u",offsets->count);
        }
        if (counts->count != offsets->count) {
          ThrowRDE("DNG Decoder: Byte count number does not match strip size: count:%u, strips:%u ",counts->count, offsets->count);
        }

        //this->readUncompressedRaw(offsets->getInt(),offsets->getInt()+counts->getInt(),0);
      } catch (TiffParserException) {
          ThrowRDE("DNG Decoder: Unsupported format.");
        // LOAD TILES??
      }
    } else if (compression == 7) {
      try {
        // Let's try loading it as tiles instead
        if (!mRaw->isCFA) {
          mRaw->bpp*=raw->getEntry(SAMPLESPERPIXEL)->getInt();
        }
        mRaw->createData();

        guint tilew = raw->getEntry(TILEWIDTH)->getInt();
        guint tileh = raw->getEntry(TILELENGTH)->getInt();
        guint tilesX = (mRaw->dim.x + tilew -1) / tilew;
        guint tilesY = (mRaw->dim.y + tileh -1) / tileh;
        int nTiles = tilesX*tilesY;

        TiffEntry *TEoffsets = raw->getEntry(TILEOFFSETS);
        const guint* offsets = TEoffsets->getIntArray();

        TiffEntry *TEcounts = raw->getEntry(TILEBYTECOUNTS);
        const guint* counts = TEcounts->getIntArray();

        if (TEoffsets->count != TEcounts->count || TEoffsets->count != nTiles)
          ThrowRDE("DNG Decoder: Tile count mismatch: offsets:%u count:%u, calculated:%u",TEoffsets->count,TEcounts->count, nTiles );

        DngDecoderSlices slices(mFile, mRaw);

        for (guint y=0; y< tilesY; y++) { // This loop is obvious for threading, as tiles are independent
          for (guint x=0; x< tilesX; x++) {
            LJpegPlain l(mFile, mRaw);
            l.mDNGCompatible = mFixLjpeg;
            DngSliceElement e(offsets[x+y*tilesX], counts[x+y*tilesX], tilew*x, tileh*y);
            slices.addSlice(e);
          }
        }
        slices.startDecoding();
        if (!slices.errors.empty())
          errors = slices.errors;
      } catch (TiffParserException) {
        TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
        TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);

        if (offsets->count != 1) {
          ThrowRDE("DNG Decoder: Multiple Strips found: %u",offsets->count);
        }
        if (counts->count != offsets->count) {
          ThrowRDE("DNG Decoder: Byte count number does not match strip size: count:%u, stips:%u ",counts->count, offsets->count);
        }

        ThrowRDE("DNG Decoder: Unsupported format.");
      }
   } else {
      ThrowRDE("DNG Decoder: Unknown compression: %u",compression);
    }
  } catch (TiffParserException) {
    ThrowRDE("DNG Decoder: Image could not be read.");
  }
  return mRaw;
}