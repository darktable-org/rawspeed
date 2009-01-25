#include "StdAfx.h"
#include "FileMap.h"

FileMap::FileMap(guint _size) : size(_size)
{
  data = (unsigned char*)_aligned_malloc(size+4,16);   
  if (!data) {
		throw new FileIOException("Not enough memory to open file.");
  }
  mOwnAlloc= true;
}

FileMap::FileMap(guchar* _data, guint _size): data(_data), size(_size) {
  mOwnAlloc = false;
}


FileMap::~FileMap(void)
{
  if (data && mOwnAlloc) {
    _aligned_free(data);
  }
  data = 0;
  size = 0;
}
