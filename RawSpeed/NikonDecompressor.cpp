#include "StdAfx.h"
#include "NikonDecompressor.h"
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

NikonDecompressor::NikonDecompressor(FileMap* file, RawImage img ) :
LJpegDecompressor(file,img)
{
  for (guint i = 0; i < 0xffff ; i++) {
    curve[i]  = i;
  }
  bits = 0;
}

NikonDecompressor::~NikonDecompressor(void)
{
  if (bits)
    delete(bits);
  bits = 0;

}
void NikonDecompressor::initTable(guint huffSelect) {
  HuffmanTable *dctbl1 = &huff[0];
  guint acc = 0;
  for (guint i = 0; i < 16 ;i++) {
    dctbl1->bits[i+1] = nikon_tree[huffSelect][i];
    acc+=dctbl1->bits[i+1];
  }
  dctbl1->bits[0] = 0;

  for(guint i =0 ; i<acc; i++) {
    dctbl1->huffval[i] = nikon_tree[huffSelect][i+16];
  }
  createHuffmanTable(dctbl1);
}

void NikonDecompressor::DecompressNikon( ByteStream &metadata, guint w, guint h, guint bitsPS, guint offset, guint size ) {
  guint v0 = metadata.getByte();
  guint v1 = metadata.getByte();
  guint huffSelect = 0;
  guint split = 0;
  gint pUp1[2];
  gint pUp2[2];
  mUseBigtable = true;

  _RPT2(0, "Nef version v0:%u, v1:%u\n",v0, v1);

  if (v0 == 73 || v1 == 88)
    metadata.skipBytes(2110);

  if (v0 == 70) huffSelect = 2;
  if (bitsPS == 14) huffSelect += 3;

  pUp1[0] = metadata.getShort();
  pUp1[1] = metadata.getShort();
  pUp2[0] = metadata.getShort();
  pUp2[1] = metadata.getShort();

  guint _max = 1 << bitsPS & 0x7fff;
  guint step = 0;
  guint csize = metadata.getShort();
  if (csize  > 1)
    step = _max / (csize-1);
  if (v0 == 68 && v1 == 32 && step > 0) {
    for (guint i=0; i < csize; i++)
      curve[i*step] = metadata.getShort();
    for (guint i=0; i < _max; i++)
      curve[i] = ( curve[i-i%step]*(step-i%step) +
      curve[i-i%step+step]*(i%step) ) / step;
    metadata.setAbsoluteOffset(562);
    split = metadata.getShort();
  } else if (v0 != 70 && csize <= 0x4001) {
    for (guint i = 0; i < csize; i++) {
      curve[i] = metadata.getShort();
    }
    _max = csize;
  }
  while (curve[_max-2] == curve[_max-1]) _max--; 
  initTable(huffSelect);

  guint x, y;
  bits = new BitPumpMSB(mFile->getData(offset), size);
  guchar *draw = mRaw->getData();
  guint *dest;
  guint pitch = mRaw->pitch;

  gint pLeft1 = 0;
  gint pLeft2 = 0;
  guint cw = w/2;

  for (y=0; y < h; y++) {
    if (split && y == split) {
      initTable(huffSelect+1);
    }
    dest = (guint*)&draw[y*pitch];  // Adjust destination
    pUp1[y&1] += HuffDecodeNikon();
    pUp2[y&1] += HuffDecodeNikon();
    pLeft1 = pUp1[y&1];
    pLeft2 = pUp2[y&1];
    dest[0] = curve[pLeft1] | (curve[pLeft2]<<16);
    for (x=1; x < cw; x++) {
      pLeft1 += HuffDecodeNikon();
      pLeft2 += HuffDecodeNikon();
      dest[x] =  curve[pLeft1] | (curve[pLeft2]<<16);
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
gint NikonDecompressor::HuffDecodeNikon()
{
  gint rv;
  gint l, temp;
  gint code,val ;

  HuffmanTable *dctbl1 = &huff[0];

  bits->fill();
  code = bits->peekBitsNoFill(14);
  val = dctbl1->bigTable[code];
  if ((val&0xff) !=  0xff) {
    bits->skipBits(val&0xff);
    return val>>8;
  }

  rv = 0;
  code = bits->peekByteNoFill();
  val = dctbl1->numbits[code];
  l = val&15;
  if (l) {
    bits->skipBits(l);
    rv=val>>4;
  }  else {
    bits->skipBits(8);
    l = 8;
    while (code > dctbl1->maxcode[l]) {
      temp = bits->getBitNoFill();
      code = (code << 1) | temp;
      l++;
    }
  
    if (l > 16) {
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
  guint len = rv & 15;
  guint shl = rv >> 4;
  gint diff =  ((bits->getBits(len-shl) << 1) + 1) << shl >> 1;
  if ((diff & (1 << (len-1))) == 0)
    diff -= (1 << len) - !shl;
  return diff;
}

