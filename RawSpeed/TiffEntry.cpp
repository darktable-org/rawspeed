#include "StdAfx.h"
#include "TiffEntry.h"
#include <math.h>


TiffEntry::TiffEntry() {
}

TiffEntry::TiffEntry(FileMap* f, guint offset) 
{
  unsigned short* p = (unsigned short*)f->getData(offset);
  tag = (TiffTag)p[0];
  type = (TiffDataType)p[1];
  count = *(int*)f->getData(offset+4);
  if (type>13)
    throw new TiffParserException("Error reading TIFF structure. Unknown Type encountered.");
  int bytesize = count << datashifts[type];
  if (bytesize <=4) {
    data = f->getDataWrt(offset+8);
  } else { // offset
    int data_offset = *(int*)f->getData(offset+8);
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
  _ASSERT(type == TIFF_LONG || type == TIFF_SHORT);
  if (type == TIFF_SHORT)
    return getShort();
  return *(unsigned int*)&data[0];
}

unsigned short TiffEntry::getShort() {
  _ASSERT(type == TIFF_SHORT);
  return *(unsigned short*)&data[0];
}

unsigned const int* TiffEntry::getIntArray() {
  _ASSERT(type == TIFF_LONG);
  return (unsigned int*)&data[0];
}

unsigned const short* TiffEntry::getShortArray() {
  _ASSERT(type == TIFF_SHORT);
  return (unsigned short*)&data[0];
}

unsigned char TiffEntry::getByte() {
  _ASSERT(type == TIFF_BYTE);
    return data[0];
}

float TiffEntry::getFloat() {
  _ASSERT(type == TIFF_FLOAT || type == TIFF_DOUBLE);
  if (type == TIFF_DOUBLE) {
    return (float)*(double*)&data[0];
  } else {
    return *(float*)&data[0];
  }
}

string TiffEntry::getString() {
  _ASSERT(type == TIFF_ASCII);
  data[count-1] = 0;  // Ensure string is not larger than count defines
  return string((char*)&data[0]);
}

int TiffEntry::getElementSize() {
    return datasizes[type];
}

int TiffEntry::getElementShift() {
    return datashifts[type];
}
