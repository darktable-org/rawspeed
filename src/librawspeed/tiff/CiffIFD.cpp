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
#include "common/Common.h"               // for uint32, ushort16
#include "io/Buffer.h"                   // for Buffer
#include "io/Endianness.h"               // for getU16LE, getU32LE
#include "io/IOException.h"              // for IOException
#include "parsers/CiffParserException.h" // for ThrowCPE, CiffParserException
#include "tiff/CiffEntry.h"              // for CiffEntry, CiffDataType::CI...
#include <limits>                        // for numeric_limits
#include <map>                           // for map, _Rb_tree_iterator
#include <memory>                        // for unique_ptr
#include <string>                        // for allocator, operator==, string
#include <utility>                       // for pair
#include <vector>                        // for vector

using namespace std;

namespace RawSpeed {

#define CIFF_DEPTH(_depth)                                                     \
  if ((depth = (_depth) + 1) > 10)                                             \
    ThrowCPE("sub-micron matryoshka dolls are ignored");

CiffIFD::CiffIFD(CiffIFD* parent_, Buffer* f, uint32 start, uint32 end,
                 uint32 _depth)
    : parent(parent_), mFile(f) {
  CIFF_DEPTH(_depth);

  checkOverflow();

  if (end < 4)
    ThrowCPE("File is probably corrupted.");

  uint32 valuedata_size = getU32LE(mFile->getData(end - 4, 4));

  if (valuedata_size >= numeric_limits<uint32>::max() - start)
    ThrowCPE("Valuedata size is too big. Image is probably corrupted.");

  ushort16 dircount = getU16LE(mFile->getData(start + valuedata_size, 2));

  //  fprintf(stderr, "Found %d entries between %d and %d after %d data
  //  bytes\n",
  //                  dircount, start, end, valuedata_size);

  for (uint32 i = 0; i < dircount; i++) {
    int entry_offset = start+valuedata_size+2+i*10;

    if (!mFile->isValid(entry_offset, 10))
      break;

    unique_ptr<CiffEntry> t;

    try {
      t = make_unique<CiffEntry>(mFile, start, entry_offset);
    } catch (IOException&) {
      // Ignore unparsable entry
      return;
    }

    try {
      switch (t->type) {
      case CIFF_SUB1:
      case CIFF_SUB2:
        add(make_unique<CiffIFD>(this, mFile, t->data_offset,
                                 t->data_offset + t->bytesize, depth));
        break;

      default:
        add(move(t));
      }
    } catch (...) {
      // Unparsable private data are added as entries
      add(move(t));
    }
  }
}

void CiffIFD::checkOverflow() {
  CiffIFD* p = this;
  for (int i = 1; p; ++i, p = p->parent)
    if (i > 5)
      ThrowCPE("CiffIFD cascading overflow.");
}

void CiffIFD::add(std::unique_ptr<CiffIFD> subIFD) {
  if (mSubIFD.size() > 100)
    ThrowCPE("CIFF file has too many SubIFDs, probably broken");

  subIFD->parent = this;
  mSubIFD.push_back(move(subIFD));
}

void CiffIFD::add(std::unique_ptr<CiffEntry> entry) {
  entry->parent = this;
  mEntry[entry->tag] = move(entry);
}

bool __attribute__((pure)) CiffIFD::hasEntryRecursive(CiffTag tag) {
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
    CiffEntry* entry = mEntry[tag].get();
    if (entry->isInt() && entry->getU32() == isValue)
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
    CiffEntry* entry = mEntry[tag].get();
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
    return mEntry[tag].get();
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
    CiffEntry* entry = mEntry[tag].get();
    if (entry->isInt() && entry->getU32() == isValue)
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
    CiffEntry* entry = mEntry[tag].get();
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
    return mEntry[tag].get();
  }
  ThrowCPE("Entry 0x%x not found.", tag);
  return nullptr;
}

bool __attribute__((pure)) CiffIFD::hasEntry(CiffTag tag) {
  return mEntry.find(tag) != mEntry.end();
}

} // namespace RawSpeed
