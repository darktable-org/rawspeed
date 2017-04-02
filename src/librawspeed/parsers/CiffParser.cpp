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
#include "common/Common.h"               // for trimSpaces
#include "decoders/CrwDecoder.h"         // for CrwDecoder
#include "io/Buffer.h"                   // for Buffer
#include "parsers/CiffParserException.h" // for ThrowCPE, CiffParserException
#include "tiff/CiffEntry.h"              // for CiffEntry
#include "tiff/CiffIFD.h"                // for CiffIFD
#include "tiff/CiffTag.h"                // for CiffTag::CIFF_MAKEMODEL
#include <map>                           // for map
#include <memory>                        // for unique_ptr
#include <string>                        // for operator==, allocator, basi...
#include <utility>                       // for pair
#include <vector>                        // for vector

using std::vector;
using std::string;

namespace RawSpeed {

class RawDecoder;

CiffParser::CiffParser(Buffer* inputData) : RawParser(inputData) {}

void CiffParser::parseData() {
  if (mInput->getSize() < 16)
    ThrowCPE("Not a CIFF file (size too small)");
  const unsigned char* data = mInput->getData(0, 16);

  if (data[0] != 0x49 || data[1] != 0x49)
    ThrowCPE("Not a CIFF file (ID)");

  mRootIFD = make_unique<CiffIFD>(nullptr, mInput, data[2], mInput->getSize());
}

RawDecoder* CiffParser::getDecoder() {
  if (!mRootIFD)
    parseData();

  vector<CiffIFD*> potentials;
  potentials = mRootIFD->getIFDsWithTag(CIFF_MAKEMODEL);

  if (!potentials.empty()) {  // We have make entry
    for (auto &potential : potentials) {
      string make = trimSpaces(potential->getEntry(CIFF_MAKEMODEL)->getString());
      if (make == "Canon") {
        return new CrwDecoder(move(mRootIFD), mInput);
      }
    }
  }

  ThrowCPE("No decoder found. Sorry.");
  return nullptr;
}

void CiffParser::MergeIFD( CiffParser* other_ciff)
{
  if (!other_ciff || !other_ciff->mRootIFD || other_ciff->mRootIFD->mSubIFD.empty())
    return;

  CiffIFD* other_root = other_ciff->mRootIFD.get();
  for (auto &i : other_root->mSubIFD) {
    mRootIFD->mSubIFD.push_back(move(i));
  }

  for (auto &i : other_root->mEntry) {
    mRootIFD->mEntry[i.first] = move(i.second);
  }
  other_root->mSubIFD.clear();
  other_root->mEntry.clear();
}

} // namespace RawSpeed
