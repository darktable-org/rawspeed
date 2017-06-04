/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2014 Pedro Côrte-Real
    Copyright (C) 2017 Roman Lebedev

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
#include "common/RawspeedException.h"    // for RawspeedException
#include "io/ByteStream.h"               // for ByteStream
#include "io/IOException.h"              // for IOException
#include "parsers/CiffParserException.h" // for ThrowCPE, CiffParserException
#include "tiff/CiffEntry.h"              // for CiffEntry, CiffDataType::CI...
#include <map>                           // for map, _Rb_tree_iterator
#include <memory>                        // for unique_ptr
#include <string>                        // for allocator, operator==, string
#include <utility>                       // for pair
#include <vector>                        // for vector

using std::string;
using std::vector;
using std::unique_ptr;

namespace rawspeed {

void CiffIFD::parseIFDEntry(ByteStream* bs) {
  unique_ptr<CiffEntry> t;

  auto origPos = bs->getPosition();

  try {
    t = make_unique<CiffEntry>(bs);
  } catch (IOException&) {
    // Ignore unparsable entry, but fix probably broken position due to
    // interruption by exception; i.e. setting it to the next entry.
    bs->setPosition(origPos + 10);
    return;
  }

  try {
    switch (t->type) {
    case CIFF_SUB1:
    case CIFF_SUB2: {
      auto subStream = bs->getSubStream(t->data_offset, t->bytesize);
      add(make_unique<CiffIFD>(this, &subStream));
      break;
    }

    default:
      add(move(t));
    }
  } catch (RawspeedException&) {
    // Unparsable private data are added as entries
    add(move(t));
  }
}

CiffIFD::CiffIFD(CiffIFD* parent_, ByteStream* mFile) : parent(parent_) {
  checkOverflow();

  if (mFile->getSize() < 4)
    ThrowCPE("File is probably corrupted.");

  mFile->setPosition(mFile->getSize() - 4);
  uint32 valuedata_size = mFile->getU32();

  mFile->setPosition(valuedata_size);
  ushort16 dircount = mFile->getU16();

  for (uint32 i = 0; i < dircount; i++)
    parseIFDEntry(mFile);
}

void CiffIFD::checkOverflow() {
  CiffIFD* p = this;
  int i = 0;
  while ((p = p->parent) != nullptr) {
    i++;
    if (i > 5)
      ThrowCPE("CiffIFD cascading overflow.");
  }
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

} // namespace rawspeed
