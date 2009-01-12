#include "StdAfx.h"
#include "LJpegPlain.h"


LJpegPlain::LJpegPlain( FileMap* file, RawImage img ) :
 LJpegDecompressor(file,img)
{

}

LJpegPlain::~LJpegPlain(void)
{
}

void LJpegPlain::decodeScan() {
  // If image attempts to decode beyond the image bounds, strip it.
  if (frame.w*frame.cps+offX > mRaw->dim.x)
    skipX = ((frame.w*frame.cps+offX)-mRaw->dim.x) / frame.cps;
  if (frame.h+offY > mRaw->dim.y)
    skipY = frame.h+offY - mRaw->dim.y;

  if (slicesW.empty())
    slicesW.push_back(frame.w*frame.cps);

  if (frame.superH != 1 || frame.superV != 1)
    ThrowRDE("LJpegDecompressor: Subsamples components not supported.");

  if (pred == 1) {
    if (frame.cps==2)
      decodeScanLeft2Comps();
    else if (frame.cps==3)
      decodeScanLeft3Comps();
    else if (frame.cps==4)
      decodeScanLeft4Comps();
    else 
      ThrowRDE("LJpegDecompressor::decodeScan: Unsupported component direction count.");
    return;
  }
  ThrowRDE("LJpegDecompressor::decodeScan: Unsupported prediction direction.");
}


#define COMPS 2
void LJpegPlain::decodeScanLeft2Comps() {
  _ASSERTE(slicesW.size()<16);  // We only have 4 bits for slice number.
  _ASSERTE(!(slicesW.size()>1 && skipX));   // Check if this is a valid state

  guchar *draw = mRaw->getData();
  // First line
  HuffmanTable *dctbl1 = &huff[frame.compInfo[0].dcTblNo];
  HuffmanTable *dctbl2 = &huff[frame.compInfo[1].dcTblNo];

  //Prepare slices (for CR2)
  guint slices =  (guint)slicesW.size()*(frame.h-skipY);
  guint *offset = new guint[slices+1];

  guint t_y = 0;
  guint t_x = 0;
  guint t_s = 0;
  guint slice = 0;
  for (slice = 0; slice< slices; slice++) {
    offset[slice] = ((t_x+offX)*sizeof(gushort)+((offY+t_y)*mRaw->pitch)) | (t_s<<28);
    _ASSERTE((offset[slice]&0x0fffffff)<mRaw->dim.x*mRaw->dim.y*mRaw->bpp);
    t_y++;
    if (t_y == (frame.h-skipY)) {
      t_y = 0;
      t_x += slicesW[t_s++];
    }
  }
  offset[slices] = offset[slices-1];        // Extra offset to avoid branch in loop.

  if (skipX)
    slicesW[slicesW.size()-1] -= skipX*frame.cps;

  // First pixels are obviously not predicted
  gint p1;
  gint p2;
  gushort *dest = (gushort*)&draw[offset[0]&0x0fffffff];
  gushort *predict = dest;
  *dest++ = p1 = (1<<(frame.prec-Pt-1)) + HuffDecode(dctbl1);
  *dest++ = p2 = (1<<(frame.prec-Pt-1)) + HuffDecode(dctbl2);

  slices =  (guint)slicesW.size();
  slice = 1;
  guint pixInSlice = slicesW[0]/COMPS-1;    // This is divided by comps, since comps pixels are processed at the time

  guint cw = (frame.w-skipX);
  gint x = 1;                            // Skip first pixels on first line.

  for (guint y=0;y<(frame.h-skipY);y++) {
    for (; x < cw ; x++) {
      p1 += HuffDecode(dctbl1);
      *dest++ = (gushort)p1;

      p2 += HuffDecode(dctbl2);
      *dest++ = (gushort)p2;

      if (0 == --pixInSlice) { // Next slice
        guint o = offset[slice++];
        dest = (gushort*)&draw[o&0x0fffffff];  // Adjust destination for next pixel
        _ASSERTE((o&0x0fffffff)<mRaw->dim.x*mRaw->dim.y*mRaw->bpp);    
        pixInSlice = slicesW[o>>28]/COMPS;
      }
      bits->checkPos();
    }
    if (skipX) {
      for (guint i = 0; i < skipX; i++) {
        HuffDecode(dctbl1);
        HuffDecode(dctbl2);
      }
    }
    p1 = predict[0];  // Predictors for next row
    p2 = predict[1];
    predict = dest;  // Adjust destination for next prediction
    x = 0;
  }
}

#undef COMPS
#define COMPS 3

void LJpegPlain::decodeScanLeft3Comps() {
  guchar *draw = mRaw->getData();
  // First line
  HuffmanTable *dctbl1 = &huff[frame.compInfo[0].dcTblNo];
  HuffmanTable *dctbl2 = &huff[frame.compInfo[1].dcTblNo];
  HuffmanTable *dctbl3 = &huff[frame.compInfo[2].dcTblNo];

  //Prepare slices (for CR2)
  guint slices =  (guint)slicesW.size()*(frame.h-skipY);
  guint *offset = new guint[slices+1];

  guint t_y = 0;
  guint t_x = 0;
  guint t_s = 0;
  guint slice = 0;
  for (slice = 0; slice< slices; slice++) {
    offset[slice] = ((t_x+offX)*sizeof(gushort)+((offY+t_y)*mRaw->pitch)) | (t_s<<28);
    _ASSERTE((offset[slice]&0x0fffffff)<mRaw->dim.x*mRaw->dim.y*mRaw->bpp);
    t_y++;
    if (t_y == (frame.h-skipY)) {
      t_y = 0;
      t_x += slicesW[t_s++];
    }
  }
  offset[slices] = offset[slices-1];        // Extra offset to avoid branch in loop.

  if (skipX)
    slicesW[slicesW.size()-1] -= skipX*frame.cps;

  // First pixels are obviously not predicted
  gint p1;
  gint p2;
  gint p3;
  gushort *dest = (gushort*)&draw[offset[0]&0x0fffffff];
  gushort *predict = dest;
  *dest++ = p1 = (1<<(frame.prec-Pt-1)) + HuffDecode(dctbl1);
  *dest++ = p2 = (1<<(frame.prec-Pt-1)) + HuffDecode(dctbl2);
  *dest++ = p3 = (1<<(frame.prec-Pt-1)) + HuffDecode(dctbl3);

  slices =  (guint)slicesW.size();
  slice = 1;
  guint pixInSlice = slicesW[0]/COMPS-1;    // This is divided by comps, since comps pixels are processed at the time

  guint cw = (frame.w-skipX);
  gint x = 1;                            // Skip first pixels on first line.

  for (guint y=0;y<(frame.h-skipY);y++) {
    for (; x < cw ; x++) {
      p1 += HuffDecode(dctbl1);
      *dest++ = (gushort)p1;

      p2 += HuffDecode(dctbl2);
      *dest++ = (gushort)p2;

      p3 += HuffDecode(dctbl3);
      *dest++ = (gushort)p3;

      if (0 == --pixInSlice) { // Next slice
        guint o = offset[slice++];
        dest = (gushort*)&draw[o&0x0fffffff];  // Adjust destination for next pixel
        _ASSERTE((o&0x0fffffff)<mRaw->dim.x*mRaw->dim.y*mRaw->bpp);    
        pixInSlice = slicesW[o>>28]/COMPS;
      }
      bits->checkPos();
    }
    if (skipX) {
      for (guint i = 0; i < skipX; i++) {
        HuffDecode(dctbl1);
        HuffDecode(dctbl2);
        HuffDecode(dctbl3);
      }
    }
    p1 = predict[0];  // Predictors for next row
    p2 = predict[1];
    p3 = predict[2];  // Predictors for next row
    predict = dest;  // Adjust destination for next prediction
    x = 0;
  }
}

#undef COMPS
#define COMPS 4

void LJpegPlain::decodeScanLeft4Comps() {
  guchar *draw = mRaw->getData();
  // First line
  HuffmanTable *dctbl1 = &huff[frame.compInfo[0].dcTblNo];
  HuffmanTable *dctbl2 = &huff[frame.compInfo[1].dcTblNo];
  HuffmanTable *dctbl3 = &huff[frame.compInfo[2].dcTblNo];
  HuffmanTable *dctbl4 = &huff[frame.compInfo[3].dcTblNo];

  //Prepare slices (for CR2)
  guint slices =  (guint)slicesW.size()*(frame.h-skipY);
  guint *offset = new guint[slices+1];

  guint t_y = 0;
  guint t_x = 0;
  guint t_s = 0;
  guint slice = 0;
  for (slice = 0; slice< slices; slice++) {
    offset[slice] = ((t_x+offX)*sizeof(gushort)+((offY+t_y)*mRaw->pitch)) | (t_s<<28);
    _ASSERTE((offset[slice]&0x0fffffff)<mRaw->dim.x*mRaw->dim.y*mRaw->bpp);
    t_y++;
    if (t_y == (frame.h-skipY)) {
      t_y = 0;
      t_x += slicesW[t_s++];
    }
  }
  offset[slices] = offset[slices-1];        // Extra offset to avoid branch in loop.

  if (skipX)
    slicesW[slicesW.size()-1] -= skipX*frame.cps;

  // First pixels are obviously not predicted
  gint p1;
  gint p2;
  gint p3;
  gint p4;
  gushort *dest = (gushort*)&draw[offset[0]&0x0fffffff];
  gushort *predict = dest;
  *dest++ = p1 = (1<<(frame.prec-Pt-1)) + HuffDecode(dctbl1);
  *dest++ = p2 = (1<<(frame.prec-Pt-1)) + HuffDecode(dctbl2);
  *dest++ = p3 = (1<<(frame.prec-Pt-1)) + HuffDecode(dctbl3);
  *dest++ = p4 = (1<<(frame.prec-Pt-1)) + HuffDecode(dctbl4);

  slices =  (guint)slicesW.size();
  slice = 1;
  guint pixInSlice = slicesW[0]/COMPS-1;    // This is divided by comps, since comps pixels are processed at the time

  guint cw = (frame.w-skipX);
  gint x = 1;                            // Skip first pixels on first line.

  for (guint y=0;y<(frame.h-skipY);y++) {
    for (; x < cw ; x++) {
      p1 += HuffDecode(dctbl1);
      *dest++ = (gushort)p1;

      p2 += HuffDecode(dctbl2);
      *dest++ = (gushort)p2;

      p3 += HuffDecode(dctbl3);
      *dest++ = (gushort)p3;

      p4 += HuffDecode(dctbl4);
      *dest++ = (gushort)p4;

      if (0 == --pixInSlice) { // Next slice
        guint o = offset[slice++];
        dest = (gushort*)&draw[o&0x0fffffff];  // Adjust destination for next pixel
        _ASSERTE((o&0x0fffffff)<mRaw->dim.x*mRaw->dim.y*mRaw->bpp);    
        pixInSlice = slicesW[o>>28]/COMPS;
      }
      bits->checkPos();
    }
    if (skipX) {
      for (guint i = 0; i < skipX; i++) {
        HuffDecode(dctbl1);
        HuffDecode(dctbl2);
        HuffDecode(dctbl3);
        HuffDecode(dctbl4);      
      }
    }
    p1 = predict[0];  // Predictors for next row
    p2 = predict[1];
    p3 = predict[2];  // Predictors for next row
    p4 = predict[3];
    predict = dest;  // Adjust destination for next prediction
    x = 0;
  }
}

void LJpegPlain::decodePentax( guint offset, guint size )
{
  // Prepare huffmann table              0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 = 16 entries
  static const guchar pentax_tree[] =  { 0,2,3,1,1,1,1,1,1,2,0,0,0,0,0,0,
                                         3,4,2,5,1,6,0,7,8,9,10,11,12 };
  //                                     0 1 2 3 4 5 6 7 8 9  0  1  2 = 13 entries
  gushort vpred[2][2] = {{0,0},{0,0}}, hpred[2];
  HuffmanTable *dctbl1 = &huff[0];
  guint acc = 0;
  for (guint i = 0; i < 16 ;i++) {
    dctbl1->bits[i+1] = pentax_tree[i];
    acc+=dctbl1->bits[i+1];
  }
  dctbl1->bits[0] = 0;

  for(guint i =0 ; i<acc; i++) {
    dctbl1->huffval[i] = pentax_tree[i+16];
  }
  createHuffmanTable(dctbl1);

  pentaxBits = new BitPumpMSB(mFile->getData(offset), size);
  guchar *draw = mRaw->getData();
  gushort *dest;
  guint w = mRaw->dim.x;
  guint h = mRaw->dim.y;
  gint diff; 

  for (guint y=0;y<h;y++) {
    dest = (gushort*)&draw[y*mRaw->pitch];  // Adjust destination
    for (guint x = 0; x < w ; x++) {
      diff = HuffDecodePentax(dctbl1);
      if (x < 2) hpred[x] = vpred[y & 1][x] += diff;
      else hpred[x & 1] += diff;
      dest[x] =  hpred[x & 1];
      _ASSERTE(0 == (hpred[x & 1] >> 12));
    }
  }
  delete pentaxBits;
}

/*
*--------------------------------------------------------------
*
* HuffDecode --
*
*	Taken from Figure F.16: extract next coded symbol from
*	input stream.  This should becode a macro.
*
* Results:
*	Next coded symbol
*
* Side effects:
*	Bitstream is parsed.
*
*--------------------------------------------------------------
*/
gint LJpegPlain::HuffDecodePentax(HuffmanTable *htbl)
{
  gint rv;
  gint l, temp;
  gint code;

  /*
  * If the huffman code is less than 8 bits, we can use the fast
  * table lookup to get its value.  It's more than 8 bits about
  * 3-4% of the time.
  */
  pentaxBits->fill();
  code = pentaxBits->peekByteNoFill();
  gint val = htbl->numbits[code];
  l = val&7;
  if (l) {
    pentaxBits->skipBits(l);
    rv=val>>3;
  }  else {
    pentaxBits->skipBits(8);
    l = 8;
    while (code > htbl->maxcode[l]) {
      temp = pentaxBits->getBitNoFill();
      code = (code << 1) | temp;
      l++;
    }

    /*
    * With garbage input we may reach the sentinel value l = 17.
    */

    if (l > 12) {
      ThrowRDE("Corrupt JPEG data: bad Huffman code:%u\n",l);
    } else {
      rv = htbl->huffval[htbl->valptr[l] +
        ((int)(code - htbl->mincode[l]))];
    }
  }
  /*
  * Section F.2.2.1: decode the difference and
  * Figure F.12: extend sign bit
  */

  if (rv) {
    gint x = pentaxBits->getBitsNoFill(rv);
    if ((x & (1 << (rv-1))) == 0)
      x -= (1 << rv) - 1;
    return x;
  } 
  return 0;
}

