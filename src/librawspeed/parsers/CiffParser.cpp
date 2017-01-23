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

#include "parsers/CiffParser.h"
#include "common/Common.h"               // for TrimSpaces
#include "decoders/CrwDecoder.h"         // for CrwDecoder
#include "parsers/CiffParserException.h" // for ThrowCPE, CiffParserException
#include "tiff/CiffEntry.h"              // for CiffEntry
#include "tiff/CiffIFD.h"                // for CiffIFD
#include "tiff/CiffTag.h"                // for CiffTag, ::CIFF_MAKEMODEL
#include <cstdio>                        // for NULL
#include <map>                           // for map, _Rb_tree_iterator, map...
#include <string>                        // for operator==, allocator, basi...
#include <utility>                       // for pair
#include <vector>                        // for vector, vector<>::iterator

using namespace std;

namespace RawSpeed {

class RawDecoder;

CiffParser::CiffParser(FileMap* inputData): mInput(inputData), mRootIFD(0) {
}

CiffParser::~CiffParser() {
  if (mRootIFD)
    delete mRootIFD;
  mRootIFD = NULL;
}

void CiffParser::parseData() {
  if (mInput->getSize() < 16)
    ThrowCPE("Not a CIFF file (size too small)");
  const unsigned char* data = mInput->getData(0, 16);

  if (data[0] != 0x49 || data[1] != 0x49)
    ThrowCPE("Not a CIFF file (ID)");

  if (mRootIFD)
    delete mRootIFD;

  mRootIFD = new CiffIFD(mInput, data[2], mInput->getSize());
}

RawDecoder* CiffParser::getDecoder() {
  if (!mRootIFD)
    parseData();

  /* Copy, so we can pass it on and not have it destroyed with ourselves */
  CiffIFD* root = mRootIFD;

  vector<CiffIFD*> potentials;
  potentials = mRootIFD->getIFDsWithTag(CIFF_MAKEMODEL);

  if (!potentials.empty()) {  // We have make entry
    for (auto &potential : potentials) {
      string make = potential->getEntry(CIFF_MAKEMODEL)->getString();
      TrimSpaces(make);
      if (make == "Canon") {
        mRootIFD = NULL;
        return new CrwDecoder(root, mInput);
      }
    }
  }

  throw CiffParserException("No decoder found. Sorry.");
  return NULL;
}

void CiffParser::MergeIFD( CiffParser* other_ciff)
{
  if (!other_ciff || !other_ciff->mRootIFD || other_ciff->mRootIFD->mSubIFD.empty())
    return;

  CiffIFD *other_root = other_ciff->mRootIFD;
  for (auto &i : other_root->mSubIFD) {
    mRootIFD->mSubIFD.push_back(i);
  }

  for (auto &i : other_root->mEntry) {
    mRootIFD->mEntry[i.first] = i.second;
  }
  other_root->mSubIFD.clear();
  other_root->mEntry.clear();
}

} // namespace RawSpeed
