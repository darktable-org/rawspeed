#include "StdAfx.h"
#include "TiffIFD.h"

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
      for (int j = 0; j < t->count; j++ ) {
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
  throw TiffParserException("TIFF Parser entry not found.");
}

bool TiffIFD::hasEntry(TiffTag tag) {
  return mEntry.find(tag) != mEntry.end();
}