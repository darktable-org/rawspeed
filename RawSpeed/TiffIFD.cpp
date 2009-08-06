#include "StdAfx.h"
#include "TiffIFD.h"
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

#ifdef CHECKSIZE
#undef CHECKSIZE
#endif

#define CHECKSIZE(A) if (A >= size) throw TiffParserException("Error reading TIFF structure. File Corrupt")

TiffIFD::TiffIFD() {
  nextIFD = 0;
}

TiffIFD::TiffIFD(FileMap* f, guint offset) 
{
  guint size = f->getSize();
  guint entries;

  entries = *(unsigned short*)f->getData(offset);    // Directory entries in this IFD

  CHECKSIZE(offset+2+entries*4);
  for (guint i = 0; i < entries; i++) {
    TiffEntry *t = new TiffEntry(f, offset+2+i*12);

    if (t->tag == SUBIFDS || t->tag == EXIFIFDPOINTER) {   // subIFD tag
      const unsigned int* sub_offsets = t->getIntArray();
      for (guint j = 0; j < t->count; j++ ) {
        mSubIFD.push_back(new TiffIFD(f, sub_offsets[j]));
      }
      delete(t);
    } else {  // Store as entry
      mEntry[t->tag] = t;
    }
  }
  nextIFD = *(int*)f->getData(offset+2+entries*12);
}

TiffIFD::~TiffIFD(void)
{
  for (map<TiffTag, TiffEntry*>::iterator i = mEntry.begin(); i != mEntry.end(); ++i) {
    delete((*i).second);
  }
  mEntry.clear();
  for (vector<TiffIFD*>::iterator i = mSubIFD.begin(); i != mSubIFD.end(); ++i) {
    delete(*i);
  }
  mSubIFD.clear();
}

vector<TiffIFD*> TiffIFD::getIFDsWithTag(TiffTag tag) {
  vector<TiffIFD*> matchingIFDs;
  if (mEntry.find(tag) != mEntry.end()) {
    matchingIFDs.push_back(this);
  }
  for (vector<TiffIFD*>::iterator i = mSubIFD.begin(); i != mSubIFD.end(); ++i) {
    vector<TiffIFD*> t = (*i)->getIFDsWithTag(tag);
    for (guint j = 0; j < t.size(); j++) {
      matchingIFDs.push_back(t[j]);
    }
  }

  return matchingIFDs;
}

TiffEntry* TiffIFD::getEntry(TiffTag tag) {
  if (mEntry.find(tag) != mEntry.end()) {
    return mEntry[tag];
  }
  ThrowTPE("TiffIFD: TIFF Parser entry 0x%x not found.", tag);
  return 0;
}

bool TiffIFD::hasEntry(TiffTag tag) {
  return mEntry.find(tag) != mEntry.end();
}