#pragma once
#include "FileIOException.h"
/*************************************************************************
 * This is the basic file map
 *
 * It allows access to a file.
 * Base implementation is for a complete file that is already in memory.
 * This can also be done as a MemMap 
 * 
 *****************************/
class FileMap
{
public:
  FileMap(guint _size);                 // Allocates the data array itself
  FileMap(guchar* _data, guint _size);  // Data already allocated.
  ~FileMap(void);
  const guchar* getData(guint offset) {return &data[offset];}
  guchar* getDataWrt(guint offset) {return &data[offset];}
  guint getSize() {return size;}
  gboolean isValid(guint offset) {return offset<=size;}
private:
 guint size;
 guchar* data;
 gboolean mOwnAlloc;
};
