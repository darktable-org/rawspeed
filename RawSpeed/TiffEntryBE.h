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
#include "TiffEntry.h"

class TiffEntryBE :
  public TiffEntry
{
public:
//  TiffEntryBE(void);
  TiffEntryBE(FileMap* f, guint offset);
  virtual ~TiffEntryBE(void);
  virtual guint getInt();
  virtual gushort getShort();
  virtual const guint* getIntArray();
  virtual const gushort* getShortArray();
private:
  bool mDataSwapped;
};

