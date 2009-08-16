#include "StdAfx.h"
#include "FileMap.h"
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

FileMap* FileMap::clone()
{
  FileMap *new_map = new FileMap(size);
  memcpy(new_map->data, data, size);
  return new_map;
}

void FileMap::corrupt( int errors )
{
  for (int i = 0; i < errors; i++) {
    guint pos = ( rand() | (rand()<<15) ) % size;
    data[pos] = rand()&0xff;
  }
}