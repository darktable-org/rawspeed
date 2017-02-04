/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "tiff/CiffIFD.h"
#include "common/Common.h"               // for uint32, getU16LE, getU32LE
#include "io/IOException.h"              // for IOException
#include "parsers/CiffParserException.h" // for ThrowCPE, CiffParserException
#include "tiff/CiffEntry.h"              // for CiffEntry, CiffDataType::CI...
#include <map>                           // for map, _Rb_tree_iterator
#include <string>                        // for allocator, operator==, string
#include <utility>                       // for pair
#include <vector>                        // for vector

using namespace std;

namespace RawSpeed {

#define CIFF_DEPTH(_depth)                                                     \
  if ((depth = (_depth) + 1) > 10)                                             \
    ThrowCPE("CIFF: sub-micron matryoshka dolls are ignored");

CiffIFD::CiffIFD(FileMap* f, uint32 start, uint32 end, uint32 _depth) {
  CIFF_DEPTH(_depth);
  mFile = f;

  uint32 valuedata_size = getU32LE(f->getData(end-4, 4));
  ushort16 dircount = getU16LE(f->getData(start+valuedata_size, 2));

//  fprintf(stderr, "Found %d entries between %d and %d after %d data bytes\n",
//                  dircount, start, end, valuedata_size);

  for (uint32 i = 0; i < dircount; i++) {
    int entry_offset = start+valuedata_size+2+i*10;

    // If the space for the entry is no longer valid stop reading any more as
    // the file is broken or truncated
    if (!mFile->isValid(entry_offset, 10))
      break;

    CiffEntry *t = nullptr;
    try {
      t = new CiffEntry(f, start, entry_offset);
    } catch (IOException &) { // Ignore unparsable entry
      continue;
    }

    if (t->type == CIFF_SUB1 || t->type == CIFF_SUB2) {
      try {
        mSubIFD.push_back(new CiffIFD(f, t->data_offset, t->data_offset+t->bytesize, depth));
        delete(t);
      } catch (
          CiffParserException &) { // Unparsable subifds are added as entries
        mEntry[t->tag] = t;
      } catch (IOException &) { // Unparsable private data are added as entries
        mEntry[t->tag] = t;
      }
    } else {
      mEntry[t->tag] = t;
    }
  }
}

CiffIFD::~CiffIFD() {
  for (auto &i : mEntry) {
    delete (i.second);
  }
  mEntry.clear();
  for (auto &i : mSubIFD) {
    delete i;
  }
  mSubIFD.clear();
}

bool CiffIFD::hasEntryRecursive(CiffTag tag) {
  if (mEntry.find(tag) != mEntry.end())
    return true;
  for (auto &i : mSubIFD) {
    if (i->hasEntryRecursive(tag))
      return true;
  }
  return false;
}

vector<CiffIFD*> CiffIFD::getIFDsWithTag(CiffTag tag) {
  vector<CiffIFD*> matchingIFDs;
  if (mEntry.find(tag) != mEntry.end()) {
    matchingIFDs.push_back(this);
  }
  for (auto &i : mSubIFD) {
    vector<CiffIFD *> t = i->getIFDsWithTag(tag);
    for (auto j : t) {
      matchingIFDs.push_back(j);
    }
  }
  return matchingIFDs;
}

vector<CiffIFD*> CiffIFD::getIFDsWithTagWhere(CiffTag tag, uint32 isValue) {
  vector<CiffIFD*> matchingIFDs;
  if (mEntry.find(tag) != mEntry.end()) {
    CiffEntry* entry = mEntry[tag];
    if (entry->isInt() && entry->getInt() == isValue)
      matchingIFDs.push_back(this);
  }
  for (auto &i : mSubIFD) {
    vector<CiffIFD *> t = i->getIFDsWithTag(tag);
    for (auto j : t) {
      matchingIFDs.push_back(j);
    }
  }
  return matchingIFDs;
}

vector<CiffIFD *> CiffIFD::getIFDsWithTagWhere(CiffTag tag,
                                               const string &isValue) {
  vector<CiffIFD*> matchingIFDs;
  if (mEntry.find(tag) != mEntry.end()) {
    CiffEntry* entry = mEntry[tag];
    if (entry->isString() && isValue == entry->getString())
      matchingIFDs.push_back(this);
  }
  for (auto &i : mSubIFD) {
    vector<CiffIFD *> t = i->getIFDsWithTag(tag);
    for (auto j : t) {
      matchingIFDs.push_back(j);
    }
  }
  return matchingIFDs;
}

CiffEntry* CiffIFD::getEntryRecursive(CiffTag tag) {
  if (mEntry.find(tag) != mEntry.end()) {
    return mEntry[tag];
  }
  for (auto &i : mSubIFD) {
    CiffEntry *entry = i->getEntryRecursive(tag);
    if (entry)
      return entry;
  }
  return nullptr;
}

CiffEntry* CiffIFD::getEntryRecursiveWhere(CiffTag tag, uint32 isValue) {
  if (mEntry.find(tag) != mEntry.end()) {
    CiffEntry* entry = mEntry[tag];
    if (entry->isInt() && entry->getInt() == isValue)
      return entry;
  }
  for (auto &i : mSubIFD) {
    CiffEntry *entry = i->getEntryRecursive(tag);
    if (entry)
      return entry;
  }
  return nullptr;
}

CiffEntry *CiffIFD::getEntryRecursiveWhere(CiffTag tag, const string &isValue) {
  if (mEntry.find(tag) != mEntry.end()) {
    CiffEntry* entry = mEntry[tag];
    if (entry->isString() && isValue == entry->getString())
      return entry;
  }
  for (auto &i : mSubIFD) {
    CiffEntry *entry = i->getEntryRecursive(tag);
    if (entry)
      return entry;
  }
  return nullptr;
}

CiffEntry* CiffIFD::getEntry(CiffTag tag) {
  if (mEntry.find(tag) != mEntry.end()) {
    return mEntry[tag];
  }
  ThrowCPE("CiffIFD: CIFF Parser entry 0x%x not found.", tag);
  return nullptr;
}


bool CiffIFD::hasEntry(CiffTag tag) {
  return mEntry.find(tag) != mEntry.end();
}

} // namespace RawSpeed
