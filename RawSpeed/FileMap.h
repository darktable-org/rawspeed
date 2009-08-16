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
  FileMap* clone();
  void corrupt(int errors);
private:
 guchar* data;
 guint size;
 gboolean mOwnAlloc;
};
