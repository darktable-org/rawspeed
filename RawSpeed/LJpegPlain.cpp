#include "StdAfx.h"
#include "LJpegPlain.h"


LJpegPlain::LJpegPlain( FileMap* file, RawImage img ) :
 LJpegDecompressor(file,img)
{
  offset = 0;
}

LJpegPlain::~LJpegPlain(void)
{
  if (offset)
    delete(offset);
}

void LJpegPlain::decodeScan() {
  // If image attempts to decode beyond the image bounds, strip it.
  if (frame.w*frame.cps+offX > mRaw->dim.x)
    skipX = ((frame.w*frame.cps+offX)-mRaw->dim.x) / frame.cps;
  if (frame.h+offY > mRaw->dim.y)
    skipY = frame.h+offY - mRaw->dim.y;

  if (slicesW.empty())
    slicesW.push_back(frame.w*frame.cps);
  
  for (guint i = 0; i < frame.cps;  i++) {
    if (frame.compInfo[i].superH != 1 || frame.compInfo[i].superV != 1)
      ThrowRDE("LJpegDecompressor: Supersampled components not supported.");
  }

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
  offset = new guint[slices+1];

  guint t_y = 0;
  guint t_x = 0;
  guint t_s = 0;
  guint slice = 0;
  for (slice = 0; slice< slices; slice++) {
    offset[slice] = ((t_x+offX)*sizeof(gushort)+((offY+t_y)*mRaw->pitch)) | (t_s<<28);
    _ASSERTE((offset[slice]&0x0fffffff)<mRaw->pitch*mRaw->dim.y);
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
  guint x = 1;                            // Skip first pixels on first line.

  for (guint y=0;y<(frame.h-skipY);y++) {
    for (; x < cw ; x++) {
      p1 += HuffDecode(dctbl1);
      *dest++ = (gushort)p1;

      p2 += HuffDecode(dctbl2);
      *dest++ = (gushort)p2;

      if (0 == --pixInSlice) { // Next slice
        guint o = offset[slice++];
        dest = (gushort*)&draw[o&0x0fffffff];  // Adjust destination for next pixel
        _ASSERTE((o&0x0fffffff)<mRaw->pitch*mRaw->dim.y);    
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
  offset = new guint[slices+1];

  guint t_y = 0;
  guint t_x = 0;
  guint t_s = 0;
  guint slice = 0;
  for (slice = 0; slice< slices; slice++) {
    offset[slice] = ((t_x+offX)*sizeof(gushort)+((offY+t_y)*mRaw->pitch)) | (t_s<<28);
    _ASSERTE((offset[slice]&0x0fffffff)<mRaw->pitch*mRaw->dim.y);
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
  guint x = 1;                            // Skip first pixels on first line.

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
        _ASSERTE((o&0x0fffffff)<mRaw->pitch*mRaw->dim.y);    
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
  offset = new guint[slices+1];

  guint t_y = 0;
  guint t_x = 0;
  guint t_s = 0;
  guint slice = 0;
  for (slice = 0; slice< slices; slice++) {
    offset[slice] = ((t_x+offX)*sizeof(gushort)+((offY+t_y)*mRaw->pitch)) | (t_s<<28);
    _ASSERTE((offset[slice]&0x0fffffff)<mRaw->pitch*mRaw->dim.y);
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
  guint x = 1;                            // Skip first pixels on first line.

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
        _ASSERTE((o&0x0fffffff)<mRaw->pitch*mRaw->dim.y);    
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
