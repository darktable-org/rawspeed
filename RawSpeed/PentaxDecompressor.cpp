#include "StdAfx.h"
#include "PentaxDecompressor.h"

PentaxDecompressor::PentaxDecompressor(FileMap* file, RawImage img ) :
LJpegDecompressor(file,img)
{
  pentaxBits = 0;
}

PentaxDecompressor::~PentaxDecompressor(void)
{
  if (pentaxBits)
    delete(pentaxBits);
  pentaxBits = 0;
}


void PentaxDecompressor::decodePentax( guint offset, guint size )
{
  // Prepare huffmann table              0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 = 16 entries
  static const guchar pentax_tree[] =  { 0,2,3,1,1,1,1,1,1,2,0,0,0,0,0,0,
    3,4,2,5,1,6,0,7,8,9,10,11,12 };
  //                                     0 1 2 3 4 5 6 7 8 9  0  1  2 = 13 entries
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
  mUseBigtable = true;
  createHuffmanTable(dctbl1);

  pentaxBits = new BitPumpMSB(mFile->getData(offset), size);
  guchar *draw = mRaw->getData();
  gushort *dest;
  guint w = mRaw->dim.x;
  guint h = mRaw->dim.y;
  gint pUp1[2] = {0,0};
  gint pUp2[2] = {0,0};
  gint pLeft1 = 0;
  gint pLeft2 = 0;

  for (guint y=0;y<h;y++) {
    dest = (gushort*)&draw[y*mRaw->pitch];  // Adjust destination
    pUp1[y&1] += HuffDecodePentax();
    pUp2[y&1] += HuffDecodePentax();
    dest[0] = pLeft1 = pUp1[y&1];
    dest[1] = pLeft2 = pUp2[y&1];
    for (guint x = 2; x < w ; x+=2) {
      pLeft1 += HuffDecodePentax();
      pLeft2 += HuffDecodePentax();
      dest[x] =  pLeft1;
      dest[x+1] =  pLeft2;
      _ASSERTE(pLeft1 >= 0 && pLeft1 <= (65536>>3));
      _ASSERTE(pLeft2 >= 0 && pLeft2 <= (65536>>3));
    }
  }
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
gint PentaxDecompressor::HuffDecodePentax()
{
  gint rv;
  gint l, temp;
  gint code,val;

  HuffmanTable *dctbl1 = &huff[0];
  /*
  * If the huffman code is less than 8 bits, we can use the fast
  * table lookup to get its value.  It's more than 8 bits about
  * 3-4% of the time.
  */
  pentaxBits->fill();
  code = pentaxBits->peekBitsNoFill(14);
  val = dctbl1->bigTable[code];
  if ((val&0xff) !=  0xff) {
    pentaxBits->skipBits(val&0xff);
    return val>>8;
  }

  code = pentaxBits->peekByteNoFill();
  val = dctbl1->numbits[code];
  l = val&15;
  if (l) {
    pentaxBits->skipBits(l);
    rv=val>>4;
  }  else {
    pentaxBits->skipBits(8);
    l = 8;
    while (code > dctbl1->maxcode[l]) {
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
      rv = dctbl1->huffval[dctbl1->valptr[l] +
        ((int)(code - dctbl1->mincode[l]))];
    }
  }

  if (rv == 16)
    return -32768;

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

