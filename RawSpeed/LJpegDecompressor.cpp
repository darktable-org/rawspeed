#include "StdAfx.h"
#include "LJpegDecompressor.h"


LJpegDecompressor::LJpegDecompressor(FileMap* file, RawImage img): mFile(file), mRaw(img)
{
  input = 0;
  skipX = skipY = 0;
  for (int i = 0; i< 4; i++) {
    huff[i].initialized = false;
  }
}

LJpegDecompressor::~LJpegDecompressor(void)
{
  if (input)
    delete input;
  input = 0;
}

void LJpegDecompressor::startDecoder(guint offset, guint size, guint offsetX, guint offsetY) {
  if (!mFile->isValid(offset+size-1))
    ThrowRDE("LJpegDecompressor::startDecoder: Max offset before out of file, invalid data");
  if (offsetX>=mRaw->dim.x)
    ThrowRDE("LJpegDecompressor::startDecoder: X offset outside of image");
  if (offsetY>=mRaw->dim.y)
    ThrowRDE("LJpegDecompressor::startDecoder: Y offset outside of image");
  offX = offsetX;
  offY = offsetY;

  try {
    input = new ByteStream(mFile->getData(offset), size);

    if (getNextMarker(false) != M_SOI)
      ThrowRDE("LJpegDecompressor::startDecoder: Image did not start with SOI. Probably not an LJPEG");
    gboolean moreImage = true;
    while (moreImage) {
        JpegMarker m = getNextMarker(true);

        switch (m) {
        case M_SOS:
          _RPT0(0,"Found SOS marker\n");
          parseSOS();
            break;
        case M_EOI:
          _RPT0(0,"Found EOI marker\n");
          moreImage = false;
          break;

        case M_DHT:
          _RPT0(0,"Found DHT marker\n");
            parseDHT();
            break;

        case M_DQT:
          ThrowRDE("LJpegDecompressor: Not a valid RAW file.");
            break;

        case M_DRI:
          _RPT0(0,"Found DRI marker\n");
            break;

        case M_APP0:
          _RPT0(0,"Found APP0 marker\n");
            break;

        case M_SOF3:
          _RPT0(0,"Found SOF 3 marker:\n");
          parseSOF();
          break;

        default:  // Just let it skip to next marker
          _RPT1(0,"Found marker:0x%x. Skipping\n",(int)m);
          break;
        }
    }
    
  } catch (IOException) {
    ThrowRDE("LJpegDecompressor: Bitpump exception, read outside file. Corrupt File.");
  }
}

void LJpegDecompressor::parseSOF() {
  guint headerLength = input->getShort();
  bpc = input->getByte();
  h = input->getShort();
  w = input->getShort();
  
  cps = input->getByte();
  
  if (bpc>16)
    ThrowRDE("LJpegDecompressor: More than 16 bits per channel is not supported.");

  if (cps>4 || cps<2)
    ThrowRDE("LJpegDecompressor: Only from 2 to 4 components are supported.");

  if(headerLength!=8+cps*3) 
    ThrowRDE("LJpegDecompressor: Header size mismatch.");

  for (guint i = 0; i< cps; i++) {
    compInfo[i].componentId = input->getByte();
    guint subs = input->getByte();
    if (subs!=0x11)
      ThrowRDE("LJpegDecompressor: Subsamples components not supported.");
    guint Tq = input->getByte();
    if (Tq!=0)
      ThrowRDE("LJpegDecompressor: Quantized components not supported.");
  }
  // If image attempts to decode beyond the image bounds, strip it.
  if (w*cps+offX > mRaw->dim.x)
    skipX = ((w*cps+offX)-mRaw->dim.x) / cps;
  if (h+offY > mRaw->dim.y)
    skipY = h+offY - mRaw->dim.y;

}

void LJpegDecompressor::parseSOS()
{
  guint headerLength = input->getShort();
  guint soscps = input->getByte();
  if (cps != soscps)
    ThrowRDE("LJpegDecompressor::parseSOS: Component number mismatch.");

  for (guint i=0;i<cps;i++) {
    guint cs = input->getByte();
    if (cs>=cps)
      ThrowRDE("LJpegDecompressor::parseSOS: Invalid Component Selector");
    guint b = input->getByte();
    guint td = b>>4;
    if (td>3)
      ThrowRDE("LJpegDecompressor::parseSOS: Invalid Huffman table selection");
    if (!huff[td].initialized)
      ThrowRDE("LJpegDecompressor::parseSOS: Invalid Huffman table selection, not defined.");

    compInfo[cs].dcTblNo = td;
    _RPT2(0,"Component Selector:%u, Table Dest:%u\n",cs, td);
  }

  pred = input->getByte();
  _RPT1(0,"Predictor:%u, ",pred);
  if (pred>7)
    ThrowRDE("LJpegDecompressor::parseSOS: Invalid predictor mode.");

  input->skipBytes(1);                    // Se + Ah Not used in LJPEG
  guint b = input->getByte();
  Pt = b&0xf;          // Point Transform
  _RPT1(0,"Point transform:%u\n",pred);
  bits = new BitPump(input);
  try {
    decodeScan();
  } catch (...) {
    delete bits;
    throw;
  }
  input->skipBytes(bits->getOffset());
  delete bits;
}

void LJpegDecompressor::parseDHT() {
  guint headerLength = input->getShort();

  guint b = input->getByte();

  guint Tc = (b>>4);
  if (Tc!=0)
    ThrowRDE("LJpegDecompressor::parseDHT: Unsupported Table class.");

  guint Th = b&0xf;
  if (Th>3)
    ThrowRDE("LJpegDecompressor::parseDHT: Invalid huffman table destination id.");

  int acc = 0;
  HuffmanTable* t = &huff[Th];

  for (guint i = 0; i < 16 ;i++) {
    t->bits[i+1] = input->getByte();
    acc+=t->bits[i+1];
  }
  t->bits[0] = 0;

  if (acc > 256) 
    ThrowRDE("LJpegDecompressor::parseDHT: Invalid DHT table.");

  if (headerLength != 3+16+acc)
    ThrowRDE("LJpegDecompressor::parseDHT: Invalid DHT table length.");

  for(int i =0 ; i<acc; i++) {
    t->huffval[i] = input->getByte();
  }
  createHuffmanTable(t);
}

void LJpegDecompressor::decodeScan() {
  if (pred == 1) {
    if (cps==2)
      decodeScanLeft2Comps();
    else if (cps==3)
      decodeScanLeft3Comps();
    else if (cps==4)
      decodeScanLeft4Comps();
    else 
      ThrowRDE("LJpegDecompressor::decodeScan: Unsupported component directioncount.");
    return;
  }
  ThrowRDE("LJpegDecompressor::decodeScan: Unsupported prediction direction.");

}


#define COMPS 2
void LJpegDecompressor::decodeScanLeft2Comps() {
  guchar *draw = mRaw->getData();
  gushort *dest = (gushort*)&draw[offX*COMPS+(offY*mRaw->pitch)];
  // First line
  HuffmanTable *dctbl1 = &huff[compInfo[0].dcTblNo];
  HuffmanTable *dctbl2 = &huff[compInfo[1].dcTblNo];

  // First two pixels are obviously not predicted
  gint p1;
  gint p2;
  dest[0] = p1 = (1<<(bpc-Pt-1)) + HuffDecode (dctbl1);
  dest[1] = p2 = (1<<(bpc-Pt-1)) + HuffDecode (dctbl2);

  guint cw = (w-skipX)*COMPS;

  for (guint y=0;y<(h-skipY);y++) {
    dest = (gushort*)&draw[offX*COMPS+(((offY+y)*mRaw->pitch))];  // Adjust destination
    guint x = y ? 0 : COMPS;   // Skip first pixels on first line.
    for (; x < cw ; x+=COMPS) {
      p1 += HuffDecode(dctbl1);
      dest[x] = (gushort)p1;

      p2 += HuffDecode(dctbl2);
      dest[x+1] = (gushort)p2;
    }
    if (skipX) {
      for (guint i = 0; i < skipX; i++) {
        HuffDecode(dctbl1);
        HuffDecode(dctbl2);
      }
    }
    p1 = dest[0];  // Predictors for next row
    p2 = dest[1];
  }
}
#undef COMPS
#define COMPS 3

void LJpegDecompressor::decodeScanLeft3Comps() {
  guchar *draw = mRaw->getData();
  gushort *dest = (gushort*)&draw[offX*COMPS+(offY*mRaw->pitch)];
  // First line
  HuffmanTable *dctbl1 = &huff[compInfo[0].dcTblNo];
  HuffmanTable *dctbl2 = &huff[compInfo[1].dcTblNo];
  HuffmanTable *dctbl3 = &huff[compInfo[2].dcTblNo];


  // First two pixels are obviously not predicted
  gint p1;
  gint p2;
  gint p3;
  dest[0] = p1 = (1<<(bpc-Pt-1)) + HuffDecode (dctbl1);
  dest[1] = p2 = (1<<(bpc-Pt-1)) + HuffDecode (dctbl2);
  dest[2] = p3 = (1<<(bpc-Pt-1)) + HuffDecode (dctbl3);

  guint cw = (w-skipX)*COMPS;

  for (guint y=0;y<(h-skipY);y++) {
    dest = (gushort*)&draw[offX*COMPS+(((offY+y)*mRaw->pitch))];  // Adjust destination
    guint x = y ? 0 : COMPS;   // Skip first pixels on first line.
    for (; x < cw ; x+=COMPS) {
      p1 += HuffDecode(dctbl1);
      dest[x] = (gushort)p1;

      p2 += HuffDecode(dctbl2);
      dest[x+1] = (gushort)p2;

      p3 += HuffDecode(dctbl3);
      dest[x+2] = (gushort)p3;

    }
    if (skipX) {
      for (guint i = 0; i < skipX; i++) {
        HuffDecode(dctbl1);
        HuffDecode(dctbl2);
        HuffDecode(dctbl3);
      }
    }
    p1 = dest[0];  // Predictors for next row
    p2 = dest[1];
    p3 = dest[2];  
  }
}

#undef COMPS
#define COMPS 4

void LJpegDecompressor::decodeScanLeft4Comps() {
  guchar *draw = mRaw->getData();
  gushort *dest = (gushort*)&draw[offX*COMPS+(offY*mRaw->pitch)];
  // First line
  HuffmanTable *dctbl1 = &huff[compInfo[0].dcTblNo];
  HuffmanTable *dctbl2 = &huff[compInfo[1].dcTblNo];
  HuffmanTable *dctbl3 = &huff[compInfo[2].dcTblNo];
  HuffmanTable *dctbl4 = &huff[compInfo[3].dcTblNo];


  // First two pixels are obviously not predicted
  gint p1;
  gint p2;
  gint p3;
  gint p4;
  dest[0] = p1 = (1<<(bpc-Pt-1)) + HuffDecode (dctbl1);
  dest[1] = p2 = (1<<(bpc-Pt-1)) + HuffDecode (dctbl2);
  dest[2] = p3 = (1<<(bpc-Pt-1)) + HuffDecode (dctbl3);
  dest[3] = p4 = (1<<(bpc-Pt-1)) + HuffDecode (dctbl4);
  guint cw = (w-skipX)*COMPS;

  for (guint y=0;y<(h-skipY);y++) {
    dest = (gushort*)&draw[offX*COMPS+(((offY+y)*mRaw->pitch))];  // Adjust destination
    guint x = y ? 0 : COMPS;   // Skip first pixels on first line.
    for (; x < cw ; x+=COMPS) {
      p1 += HuffDecode(dctbl1);
      dest[x] = (gushort)p1;

      p2 += HuffDecode(dctbl2);
      dest[x+1] = (gushort)p2;

      p3 += HuffDecode(dctbl3);
      dest[x+2] = (gushort)p3;

      p4 += HuffDecode(dctbl4);
      dest[x+3] = (gushort)p4;
    }
    if (skipX) {
      for (guint i = 0; i < skipX; i++) {
        HuffDecode(dctbl1);
        HuffDecode(dctbl2);
        HuffDecode(dctbl3);
        HuffDecode(dctbl4);
      }
    }
    p1 = dest[0];  // Predictors for next row
    p2 = dest[1];
    p3 = dest[2];  
    p4 = dest[3];
  }
}
JpegMarker LJpegDecompressor::getNextMarker(bool allowskip) {

  if (!allowskip) {
    guchar id = input->getByte();
    if (id != 0xff)
      ThrowRDE("LJpegDecompressor::getNextMarker: (Noskip) Expected marker not found. Propably corrupt file.");

    JpegMarker mark = (JpegMarker)input->getByte();

    if (M_FILL == mark || M_STUFF == mark)
      ThrowRDE("LJpegDecompressor::getNextMarker: (Noskip) Expected marker, but found stuffed 00 or ff.");

    return mark;
  }
  guint skipped = 0;
  input->skipToMarker();
  guchar id = input->getByte();
  _ASSERTE(0xff == id);
  JpegMarker mark = (JpegMarker)input->getByte();
  return mark;
}

void LJpegDecompressor::createHuffmanTable(HuffmanTable *htbl) {
  gint p, i, l, lastp, si;
  gchar huffsize[257];
  gushort huffcode[257];
  gushort code;
  gint size;
  gint value, ll, ul;

  /*
  * Figure C.1: make table of Huffman code length for each symbol
  * Note that this is in code-length order.
  */
  p = 0;
  for (l = 1; l <= 16; l++) {
    for (i = 1; i <= (int)htbl->bits[l]; i++)
      huffsize[p++] = (gchar)l;
  }
  huffsize[p] = 0;
  lastp = p;


  /*
  * Figure C.2: generate the codes themselves
  * Note that this is in code-length order.
  */
  code = 0;
  si = huffsize[0];
  p = 0;
  while (huffsize[p]) {
    while (((int)huffsize[p]) == si) {
      huffcode[p++] = code;
      code++;
    }
    code <<= 1;
    si++;
  }


  /*
  * Figure F.15: generate decoding tables
  */
  htbl->mincode[0] = 0;
  htbl->maxcode[0] = 0;
  p = 0;
  for (l = 1; l <= 16; l++) {
    if (htbl->bits[l]) {
      htbl->valptr[l] = p;
      htbl->mincode[l] = huffcode[p];
      p += htbl->bits[l];
      htbl->maxcode[l] = huffcode[p - 1];
    } else {
      htbl->maxcode[l] = -1;
    }
  }

  /*
  * We put in this value to ensure HuffDecode terminates.
  */
  htbl->maxcode[17] = 0xFFFFFL;

  /*
  * Build the numbits, value lookup tables.
  * These table allow us to gather 8 bits from the bits stream,
  * and immediately lookup the size and value of the huffman codes.
  * If size is zero, it means that more than 8 bits are in the huffman
  * code (this happens about 3-4% of the time).
  */
  memset (htbl->numbits, 0, sizeof(htbl->numbits));
  for (p=0; p<lastp; p++) {
    size = huffsize[p];
    if (size <= 8) {
      value = htbl->huffval[p];
      code = huffcode[p];
      ll = code << (8-size);
      if (size < 8) {
        ul = ll | bitMask[24+size];
      } else {
        ul = ll;
      }
      _ASSERTE(ll >= 0 && ul >=0);
      _ASSERTE(ll < 256 && ul < 256);
      _ASSERTE(ll <= ul);
      for (i=ll; i<=ul; i++) {
        htbl->numbits[i] = size | (value<<3);
        //htbl->value[i] = value;
      }
    }
  }
  htbl->initialized = true;
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
__inline gint LJpegDecompressor::HuffDecode(HuffmanTable *htbl)
{
  gint rv;
  gint l, temp;
  gint code;

  /*
  * If the huffman code is less than 8 bits, we can use the fast
  * table lookup to get its value.  It's more than 8 bits about
  * 3-4% of the time.
  */
  bits->fill();
  code = bits->peekByteNoFill();
  gint val = htbl->numbits[code];
  l = val&7;
  if (l) {
    bits->skipBits(l);
    rv=val>>3;
  }  else {
    bits->skipBits(8);
    l = 8;
    while (code > htbl->maxcode[l]) {
      temp = bits->getBitNoFill();
      code = (code << 1) | temp;
      l++;
    }

    /*
    * With garbage input we may reach the sentinel value l = 17.
    */

    if (l > 16) {
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
  if ((rv+l)>24)  // Ensure we have enough bits
    bits->fill();

  if (rv) {
    gint x = bits->getBitsNoFill(rv);
    gint t = extendTest[rv];
    if ((x) < (t&0xffff)) {
      (x) += t>>16;
    }
    return x;
  } 
  return 0;
}
