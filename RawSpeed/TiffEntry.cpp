#include "StdAfx.h"
#include "TiffEntry.h"
#include <math.h>
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


TiffEntry::TiffEntry() {
}

TiffEntry::TiffEntry(FileMap* f, guint offset) 
{
  unsigned short* p = (unsigned short*)f->getData(offset);
  tag = (TiffTag)p[0];
  type = (TiffDataType)p[1];
  count = *(int*)f->getData(offset+4);
  if (type>13)
    throw TiffParserException("Error reading TIFF structure. Unknown Type encountered.");
  int bytesize = count << datashifts[type];
  if (bytesize <=4) {
    data = f->getDataWrt(offset+8);
  } else { // offset
    guint data_offset = *(guint*)f->getData(offset+8);
    CHECKSIZE(data_offset+bytesize);
    data = f->getDataWrt(data_offset);
  }
#ifdef _DEBUG
  debug_intVal = 0xC0C4C014;
  debug_floatVal = sqrtf(-1);

  if (type == TIFF_LONG || type == TIFF_SHORT) 
    debug_intVal = getInt();
  if (type == TIFF_FLOAT || type == TIFF_DOUBLE) 
    debug_floatVal = getFloat();
#endif
}

TiffEntry::~TiffEntry(void)
{
}

unsigned int TiffEntry::getInt() {
  if(!(type == TIFF_LONG || type == TIFF_SHORT))
    throw TiffParserException("TIFF, getInt: Wrong type encountered. Expected Long");
  if (type == TIFF_SHORT)
    return getShort();
  return *(unsigned int*)&data[0];
}

unsigned short TiffEntry::getShort() {
  if (type != TIFF_SHORT)
    throw TiffParserException("TIFF, getShort: Wrong type encountered. Expected Short");
  return *(unsigned short*)&data[0];
}

unsigned const int* TiffEntry::getIntArray() {
  if (type != TIFF_LONG)
    throw TiffParserException("TIFF, getIntArray: Wrong type encountered. Expected Long");
  return (unsigned int*)&data[0];
}

unsigned const short* TiffEntry::getShortArray() {
  if (type != TIFF_SHORT)
    throw TiffParserException("TIFF, getShortArray: Wrong type encountered. Expected Short");
  return (unsigned short*)&data[0];
}

unsigned char TiffEntry::getByte() {
  if (type != TIFF_BYTE)
    throw TiffParserException("TIFF, getByte: Wrong type encountered. Expected Byte");
  return data[0];
}

float TiffEntry::getFloat() {
  if (!(type == TIFF_FLOAT || type == TIFF_DOUBLE))
    throw TiffParserException("TIFF, getFloat: Wrong type encountered. Expected Float");
  if (type == TIFF_DOUBLE) {
    return (float)*(double*)&data[0];
  } else {
    return *(float*)&data[0];
  }
}

string TiffEntry::getString() {
  if (type != TIFF_ASCII)
    throw TiffParserException("TIFF, getString: Wrong type encountered. Expected Ascii");
  data[count-1] = 0;  // Ensure string is not larger than count defines
  return string((char*)&data[0]);
}

int TiffEntry::getElementSize() {
    return datasizes[type];
}

int TiffEntry::getElementShift() {
    return datashifts[type];
}
