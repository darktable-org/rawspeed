#include "StdAfx.h"
#include "TiffEntryBE.h"

TiffEntryBE::TiffEntryBE(void)
{
}

TiffEntryBE::TiffEntryBE(FileMap* f, guint offset) : mDataSwapped(false)
{
  type = TIFF_UNDEFINED;  // We set type to undefined to avoid debug assertion errors.
  data = f->getDataWrt(offset);
  tag = (TiffTag)getShort();
  data +=2;
  TiffDataType _type = (TiffDataType)getShort();
  data +=2;
  count = getInt();
  type = _type;         //Now we can set it to the proper type

  if (type>13)
    throw new TiffParserException("Error reading TIFF structure. Unknown Type encountered.");
  int bytesize = count << datashifts[type];
  if (bytesize <=4) {
    data = f->getDataWrt(offset+8);
  } else { // offset
    data = f->getDataWrt(offset+8);
    int data_offset = (unsigned int)data[0] << 24 | (unsigned int)data[1] << 16 | (unsigned int)data[2] << 8 | (unsigned int)data[3];
    CHECKSIZE(data_offset+bytesize);
    data = f->getDataWrt(data_offset);
  }
}

TiffEntryBE::~TiffEntryBE(void)
{
}

unsigned int TiffEntryBE::getInt() {
  _ASSERT(type == TIFF_LONG || type == TIFF_SHORT || type == TIFF_UNDEFINED);
  if (type == TIFF_SHORT)
    return getShort();
  return  (unsigned int)data[0] << 24 | (unsigned int)data[1] << 16 | (unsigned int)data[2] << 8 | (unsigned int)data[3];
}

unsigned short TiffEntryBE::getShort() {
  _ASSERT(type == TIFF_SHORT || type == TIFF_UNDEFINED);
  return (unsigned short)data[0] << 8 | (unsigned short)data[1];
}

const unsigned int* TiffEntryBE::getIntArray() {
  //TODO: Make critical section to avoid clashes.
  _ASSERT(type == TIFF_LONG || type == TIFF_UNDEFINED);
  if (mDataSwapped) 
    return (unsigned int*)&data[0];

  unsigned int* d = (unsigned int*)&data[0];
  for (int i = 0; i < count; i++) {
    d[i] = (unsigned int)data[i*4+0] << 24 | (unsigned int)data[i*4+1] << 16 | (unsigned int)data[i*4+2] << 8 | (unsigned int)data[i*4+3];
  }
  mDataSwapped = true;
  return d;
}

const unsigned short* TiffEntryBE::getShortArray() {
   //TODO: Make critical section to avoid clashes.
 _ASSERT(type == TIFF_SHORT || type == TIFF_UNDEFINED);
  if (mDataSwapped) 
    return (unsigned short*)&data[0];

  unsigned short* d = (unsigned short*)&data[0];
  for (int i = 0; i < count; i++) {
    d[i] = (unsigned short)data[i*2+0] << 8 | (unsigned short)data[i*2+1];
  }
  mDataSwapped = true;
  return d;
}