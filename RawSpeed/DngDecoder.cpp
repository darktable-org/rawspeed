#include "StdAfx.h"
#include "DngDecoder.h"
#include <iostream>

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

//#define PRINT_INFO

DngDecoder::DngDecoder(TiffIFD *rootIFD, FileMap* file) : RawDecoder(file), mRootIFD(rootIFD) {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(DNGVERSION);
  const unsigned char* v = data[0]->getEntry(DNGVERSION)->getData();

  if (v[0] != 1)
    ThrowRDE("Not a supported DNG image format: v%u.%u.%u.%u", (int)v[0], (int)v[1], (int)v[2], (int)v[3]);
  if (v[1] > 3)
    ThrowRDE("Not a supported DNG image format: v%u.%u.%u.%u", (int)v[0], (int)v[1], (int)v[2], (int)v[3]);

  if ((v[0] <= 1) && (v[1] < 1))  // Prior to v1.1.xxx  fix LJPEG encoding bug
    mFixLjpeg = true;
  else
    mFixLjpeg = false;
}

DngDecoder::~DngDecoder(void) {
}

RawImage DngDecoder::decodeRaw() {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(COMPRESSION);

  if (data.empty())
    ThrowRDE("DNG Decoder: No image data found");

  // Erase the ones not with JPEG compression
  for (vector<TiffIFD*>::iterator i = data.begin(); i != data.end();) {
    int compression = (*i)->getEntry(COMPRESSION)->getShort();
    bool isSubsampled = false;
    try {
      isSubsampled = (*i)->getEntry(NEWSUBFILETYPE)->getInt() & 1; // bit 0 is on if image is subsampled
    } catch (TiffParserException) {}
    if ((compression != 7 && compression != 1) || isSubsampled) {  // Erase if subsampled, or not JPEG or uncompressed
      i = data.erase(i);
    } else {
      i++;
    }
  }

  if (data.empty())
    ThrowRDE("DNG Decoder: No RAW chunks found");

  if (data.size() > 1) {
    _RPT0(0, "Multiple RAW chunks found - using first only!");
  }

  TiffIFD* raw = data[0];
  mRaw = RawImage::create();
  mRaw->isCFA = (raw->getEntry(PHOTOMETRICINTERPRETATION)->getShort() == 32803);

  if (mRaw->isCFA)
    _RPT0(0, "This is a CFA image\n");
  else
    _RPT0(0, "This is NOT a CFA image\n");

  try {
    mRaw->dim.x = raw->getEntry(IMAGEWIDTH)->getInt();
    mRaw->dim.y = raw->getEntry(IMAGELENGTH)->getInt();
    mRaw->bpp = 2;
  } catch (TiffParserException) {
    ThrowRDE("DNG Decoder: Could not read basic image information.");
  }

  try {

    int compression = raw->getEntry(COMPRESSION)->getShort();
    if (mRaw->isCFA) {

      // Check if layout is OK, if present
      if (raw->hasEntry(CFALAYOUT))
        if (raw->getEntry(CFALAYOUT)->getShort() != 1)
          ThrowRDE("DNG Decoder: Unsupported CFA Layout.");

      const unsigned short* pDim = raw->getEntry(CFAREPEATPATTERNDIM)->getShortArray(); // Get the size
      const unsigned char* cPat = raw->getEntry(CFAPATTERN)->getData();                 // Does NOT contain dimensions as some documents state
      /*
            if (raw->hasEntry(CFAPLANECOLOR)) {
              TiffEntry* e = raw->getEntry(CFAPLANECOLOR);
              const unsigned char* cPlaneOrder = e->getData();       // Map from the order in the image, to the position in the CFA
              printf("Planecolor: ");
              for (guint i = 0; i < e->count; i++) {
                printf("%u,",cPlaneOrder[i]);
              }
              printf("\n");
            }
      */
      iPoint2D cfaSize(pDim[1], pDim[0]);
      if (pDim[0] != 2)
        ThrowRDE("DNG Decoder: Unsupported CFA configuration.");
      if (pDim[1] != 2)
        ThrowRDE("DNG Decoder: Unsupported CFA configuration.");

      if (cfaSize.area() != raw->getEntry(CFAPATTERN)->count)
        ThrowRDE("DNG Decoder: CFA pattern dimension and pattern count does not match: %d.");

      for (int y = 0; y < cfaSize.y; y++) {
        for (int x = 0; x < cfaSize.x; x++) {
          guint c1 = cPat[x+y*cfaSize.x];
          CFAColor c2;
          switch (c1) {
            case 0:
              c2 = CFA_RED; break;
            case 1:
              c2 = CFA_GREEN; break;
            case 2:
              c2 = CFA_BLUE; break;
            default:
              c2 = CFA_UNKNOWN;
              ThrowRDE("DNG Decoder: Unsupported CFA Color.");
          }
          mRaw->cfa.setColorAt(iPoint2D(x, y), c2);
        }
      }
    }


    // Now load the image
    if (compression == 1) {  // Uncompressed.
      try {
        if (!mRaw->isCFA)
          mRaw->setCpp(raw->getEntry(SAMPLESPERPIXEL)->getInt());
        guint nslices = raw->getEntry(STRIPOFFSETS)->count;
        TiffEntry *TEoffsets = raw->getEntry(STRIPOFFSETS);
        TiffEntry *TEcounts = raw->getEntry(STRIPBYTECOUNTS);
        const guint* offsets = TEoffsets->getIntArray();
        const guint* counts = TEcounts->getIntArray();
        guint yPerSlice = raw->getEntry(ROWSPERSTRIP)->getInt();
        guint width = raw->getEntry(IMAGEWIDTH)->getInt();
        guint height = raw->getEntry(IMAGELENGTH)->getInt();
        guint bps = raw->getEntry(BITSPERSAMPLE)->getShort();

        if (TEcounts->count != TEoffsets->count) {
          ThrowRDE("DNG Decoder: Byte count number does not match strip size: count:%u, strips:%u ", TEcounts->count, TEoffsets->count);
        }

        guint offY = 0;
        vector<DngStrip> slices;
        for (guint s = 0; s < nslices; s++) {
          DngStrip slice;
          slice.offset = offsets[s];
          slice.count = counts[s];
          slice.offsetY = offY;
          if (offY + yPerSlice > height)
            slice.h = height - offY;
          else
            slice.h = yPerSlice;

          offY += yPerSlice;

          if (mFile->isValid(slice.offset + slice.count)) // Only decode if size is valid
            slices.push_back(slice);
        }

        mRaw->createData();

        for (guint i = 0; i < slices.size(); i++) {
          DngStrip slice = slices[i];
          ByteStream in(mFile->getData(slice.offset), slice.count);
          iPoint2D size(width, slice.h);
          iPoint2D pos(0, slice.offsetY);

          gboolean big_endian = (raw->endian == big);
          // DNG spec says that if not 8 or 16 bit/sample, always use big endian
          if (bps != 8 && bps != 16)
            big_endian = true;

          readUncompressedRaw(in, size, pos, width*bps / 8, bps, big_endian);
        }

      } catch (TiffParserException) {
        ThrowRDE("DNG Decoder: Unsupported format, uncompressed with no strips.");
      }
    } else if (compression == 7) {
      try {
        // Let's try loading it as tiles instead

        if (!mRaw->isCFA) {
          mRaw->setCpp(raw->getEntry(SAMPLESPERPIXEL)->getInt());
        }
        mRaw->createData();

        DngDecoderSlices slices(mFile, mRaw);
        if (raw->hasEntry(TILEOFFSETS)) {
          guint tilew = raw->getEntry(TILEWIDTH)->getInt();
          guint tileh = raw->getEntry(TILELENGTH)->getInt();
          guint tilesX = (mRaw->dim.x + tilew - 1) / tilew;
          guint tilesY = (mRaw->dim.y + tileh - 1) / tileh;
          guint nTiles = tilesX * tilesY;

          TiffEntry *TEoffsets = raw->getEntry(TILEOFFSETS);
          const guint* offsets = TEoffsets->getIntArray();

          TiffEntry *TEcounts = raw->getEntry(TILEBYTECOUNTS);
          const guint* counts = TEcounts->getIntArray();

          if (TEoffsets->count != TEcounts->count || TEoffsets->count != nTiles)
            ThrowRDE("DNG Decoder: Tile count mismatch: offsets:%u count:%u, calculated:%u", TEoffsets->count, TEcounts->count, nTiles);

          slices.mFixLjpeg = mFixLjpeg;

          for (guint y = 0; y < tilesY; y++) {
            for (guint x = 0; x < tilesX; x++) {
              DngSliceElement e(offsets[x+y*tilesX], counts[x+y*tilesX], tilew*x, tileh*y);
              e.mUseBigtable = tilew * tileh > 1024 * 1024;
              slices.addSlice(e);
            }
          }
        } else {  // Strips
          TiffEntry *TEoffsets = raw->getEntry(STRIPOFFSETS);
          TiffEntry *TEcounts = raw->getEntry(STRIPBYTECOUNTS);

          const guint* offsets = TEoffsets->getIntArray();
          const guint* counts = TEcounts->getIntArray();
          guint yPerSlice = raw->getEntry(ROWSPERSTRIP)->getInt();

          if (TEcounts->count != TEoffsets->count) {
            ThrowRDE("DNG Decoder: Byte count number does not match strip size: count:%u, stips:%u ", TEcounts->count, TEoffsets->count);
          }

          guint offY = 0;
          for (guint s = 0; s < TEcounts->count; s++) {
            DngSliceElement e(offsets[s], counts[s], 0, offY);
            e.mUseBigtable = yPerSlice * mRaw->dim.y > 1024 * 1024;
            offY += yPerSlice;

            if (mFile->isValid(e.byteOffset + e.byteCount)) // Only decode if size is valid
              slices.addSlice(e);
          }
        }
        guint nSlices = slices.size();
        if (!nSlices)
          ThrowRDE("DNG Decoder: No valid slices found.");

        slices.startDecoding();

        if (!slices.errors.empty())
          errors = slices.errors;

        if (errors.size() >= nSlices)
          ThrowRDE("DNG Decoding: Too many errors encountered. Giving up.\nFirst Error:%s", errors[0]);
      } catch (TiffParserException e) {
        ThrowRDE("DNG Decoder: Unsupported format, tried strips and tiles:\n%s", e.what());
      }
    } else {
      ThrowRDE("DNG Decoder: Unknown compression: %u", compression);
    }
  } catch (TiffParserException e) {
    ThrowRDE("DNG Decoder: Image could not be read:\n%s", e.what());
  }
  iPoint2D new_size(mRaw->dim.x, mRaw->dim.y);
#ifndef PRINT_INFO
  // Crop

  if (raw->hasEntry(ACTIVEAREA)) {
    const guint *corners = raw->getEntry(ACTIVEAREA)->getIntArray();
    iPoint2D top_left(corners[1], corners[0]);
    new_size = iPoint2D(corners[3] - corners[1], corners[2] - corners[0]);
    mRaw->subFrame(top_left, new_size);

  } else if (raw->hasEntry(DEFAULTCROPORIGIN)) {

    iPoint2D top_left(0, 0);

    if (raw->getEntry(DEFAULTCROPORIGIN)->type == TIFF_LONG) {
      const guint* tl = raw->getEntry(DEFAULTCROPORIGIN)->getIntArray();
      const guint* sz = raw->getEntry(DEFAULTCROPSIZE)->getIntArray();
      top_left = iPoint2D(tl[0], tl[1]);
      new_size = iPoint2D(sz[0], sz[1]);
    } else if (raw->getEntry(DEFAULTCROPORIGIN)->type == TIFF_SHORT) {
      const gushort* tl = raw->getEntry(DEFAULTCROPORIGIN)->getShortArray();
      const gushort* sz = raw->getEntry(DEFAULTCROPSIZE)->getShortArray();
      top_left = iPoint2D(tl[0], tl[1]);
      new_size = iPoint2D(sz[0], sz[1]);
    }
    mRaw->subFrame(top_left, new_size);
  }
#endif
  // Linearization

  if (raw->hasEntry(LINEARIZATIONTABLE)) {
    const gushort* intable = raw->getEntry(LINEARIZATIONTABLE)->getShortArray();
    guint len =  raw->getEntry(LINEARIZATIONTABLE)->count;
    gushort table[65536];
    for (guint i = 0; i < 65536 ; i++) {
      if (i < len)
        table[i] = intable[i];
      else
        table[i] = intable[len-1];
    }
    for (gint y = 0; y < mRaw->dim.y; y++) {
      guint cw = mRaw->dim.x * mRaw->getCpp();
      gushort* pixels = (gushort*)mRaw->getData(0, y);
      for (guint x = 0; x < cw; x++) {
        pixels[x]  = table[pixels[x]];
      }
    }
  }
  mRaw->whitePoint = raw->getEntry(WHITELEVEL)->getInt();

  int black = -1; // Estimate, if no blacklevel
  if (raw->hasEntry(BLACKLEVELREPEATDIM)) {
    black = 65536;
    const gushort *blackdim = raw->getEntry(BLACKLEVELREPEATDIM)->getShortArray();
    if (blackdim[0] != 0 && blackdim[1] != 0) {
      if (raw->hasEntry(BLACKLEVELDELTAV)) {
        const guint *blackarray = raw->getEntry(BLACKLEVEL)->getIntArray();
        int blackbase = blackarray[0] / blackarray[1];
        const gint *blackarrayv = (const gint*)raw->getEntry(BLACKLEVELDELTAV)->getIntArray();
        for (int i = 0; i < new_size.y; i++)
          if (blackarrayv[i*2+1])
            black = MIN(black, blackbase + blackarrayv[i*2] / blackarrayv[i*2+1]);
      } else {
        TiffEntry* black_entry = raw->getEntry(BLACKLEVEL);
        if (black_entry->type == TIFF_LONG) {
          const guint* blackarray = black_entry->getIntArray();
          if (blackarray[1])
            black = blackarray[0] / blackarray[1];
          else
            black = 0;
        } else if (black_entry->type == TIFF_RATIONAL) {
          const guint* blackarray = (const guint*)black_entry->getData();
          if (blackarray[1])
            black = blackarray[0] / blackarray[1];
          else
            black = 0;
        }
      }
    } else {
      black = 0;
    }
  }
  mRaw->blackLevel = black;
  return mRaw;
}

void DngDecoder::decodeMetaData(CameraMetaData *meta) {
#ifdef PRINT_INFO
  if (mRaw->isCFA)
    printMetaData();
#endif
}

void DngDecoder::checkSupport(CameraMetaData *meta) {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);
  if (data.empty())
    ThrowRDE("DNG Support check: Model name found");
  string make = data[0]->getEntry(MAKE)->getString();
  string model = data[0]->getEntry(MODEL)->getString();
  this->checkCameraSupported(meta, make, model, "dng");
}

void DngDecoder::printMetaData() {
  vector<TiffIFD*> data = mRootIFD->getIFDsWithTag(MODEL);
  if (data.empty())
    ThrowRDE("Model name found");
  TiffIFD* raw = data[0];

  string model = raw->getEntry(MODEL)->getString();
  string make = raw->getEntry(MAKE)->getString();
  TrimSpaces(model);
  TrimSpaces(make);

  data = mRootIFD->getIFDsWithTag(COMPRESSION);

  if (data.empty())
    ThrowRDE("DNG Decoder: No image data found");

  // Erase the ones not with JPEG compression
  for (vector<TiffIFD*>::iterator i = data.begin(); i != data.end();) {
    int compression = (*i)->getEntry(COMPRESSION)->getShort();
    bool isSubsampled = false;
    try {
      isSubsampled = (*i)->getEntry(NEWSUBFILETYPE)->getInt() & 1; // bit 0 is on if image is subsampled
    } catch (TiffParserException) {}
    if ((compression != 7 && compression != 1) || isSubsampled) {  // Erase if subsampled, or not JPEG or uncompressed
      i = data.erase(i);
    } else {
      i++;
    }
  }

  if (data.empty())
    ThrowRDE("RAW section not found");

  raw = data[0];
  ColorFilterArray cfa(mRaw->cfa);

  // Crop
  iPoint2D top_left(0, 0);
  iPoint2D new_size(mRaw->dim.x, mRaw->dim.y);

  if (raw->hasEntry(ACTIVEAREA)) {
    const guint *corners = raw->getEntry(ACTIVEAREA)->getIntArray();
    top_left = iPoint2D(corners[1], corners[0]);
    new_size = iPoint2D(corners[3] - corners[1], corners[2] - corners[0]);

  } else if (raw->hasEntry(DEFAULTCROPORIGIN)) {

    if (raw->getEntry(DEFAULTCROPORIGIN)->type == TIFF_LONG) {
      const guint* tl = raw->getEntry(DEFAULTCROPORIGIN)->getIntArray();
      const guint* sz = raw->getEntry(DEFAULTCROPSIZE)->getIntArray();
      top_left = iPoint2D(tl[0], tl[1]);
      new_size = iPoint2D(sz[0], sz[1]);
    } else if (raw->getEntry(DEFAULTCROPORIGIN)->type == TIFF_SHORT) {
      const gushort* tl = raw->getEntry(DEFAULTCROPORIGIN)->getShortArray();
      const gushort* sz = raw->getEntry(DEFAULTCROPSIZE)->getShortArray();
      top_left = iPoint2D(tl[0], tl[1]);
      new_size = iPoint2D(sz[0], sz[1]);
    }
  }

  if (top_left.x & 1)
    cfa.shiftLeft();
  if (top_left.y & 1)
    cfa.shiftDown();

  int black = -1;
  if (raw->hasEntry(BLACKLEVELREPEATDIM)) {
    const gushort *blackdim = raw->getEntry(BLACKLEVELREPEATDIM)->getShortArray();
    black = 65536;
    if (blackdim[0] != 0 && blackdim[1] != 0) {
      if (raw->hasEntry(BLACKLEVELDELTAV)) {
        const guint *blackarray = raw->getEntry(BLACKLEVEL)->getIntArray();
        int blackbase = blackarray[0] / blackarray[1];
        const gint *blackarrayv = (const gint*)raw->getEntry(BLACKLEVELDELTAV)->getIntArray();
        for (int i = 0; i < new_size.y; i++)
          if (blackarrayv[i*2+1])
            black = MIN(black, blackbase + blackarrayv[i*2] / blackarrayv[i*2+1]);
      } else {
        const guint *blackarray = raw->getEntry(BLACKLEVEL)->getIntArray();
        if (blackarray[1])
          black = blackarray[0] / blackarray[1];
        else
          black = 0;
      }
    } else {
      black = 0;
    }
  }

  cout << "<Camera make=\"" << make << "\" model = \"" << model << "\">" << endl;
  cout << "<CFA width=\"2\" height=\"2\">" << endl;
  cout << "<Color x=\"0\" y=\"0\">" << ColorFilterArray::colorToString(cfa.getColorAt(0, 0)) << "</Color>";
  cout << "<Color x=\"1\" y=\"0\">" << ColorFilterArray::colorToString(cfa.getColorAt(1, 0)) << "</Color>" << endl;
  cout << "<Color x=\"0\" y=\"1\">" << ColorFilterArray::colorToString(cfa.getColorAt(0, 1)) << "</Color>";
  cout << "<Color x=\"1\" y=\"1\">" << ColorFilterArray::colorToString(cfa.getColorAt(1, 1)) << "</Color>" << endl;
  cout << "</CFA>" << endl;
  cout << "<Crop x=\"" << top_left.x << "\" y=\"" << top_left.y << "\" ";
  cout << "width=\"" << new_size.x << "\" height=\"" << new_size.y << "\"/>" << endl;
  int white = raw->getEntry(WHITELEVEL)->getInt();

  cout << "<Sensor black=\"" << black << "\" white=\"" << white << "\"/>" << endl;

  cout << "</Camera>" << endl;

}

} // namespace RawSpeed
