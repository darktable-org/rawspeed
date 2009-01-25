#pragma once
#include "RawDecoder.h"
#include <queue>
#include "pthread.h"
#include "LJpegPlain.h"

class DngSliceElement
{
public:
  DngSliceElement(guint off, guint count, guint offsetX, guint offsetY) : 
      byteOffset(off), byteCount(count), offX(offsetX), offY(offsetY) {};
  ~DngSliceElement(void) {};
  const guint byteOffset;
  const guint byteCount;
  const guint offX;
  const guint offY;
};
class DngDecoderSlices;

class DngDecoderThread
{
public:
  DngDecoderThread(void) {}
  ~DngDecoderThread(void) {}
  pthread_t threadid;
  queue<DngSliceElement> slices;
  DngDecoderSlices* parent;
};


class DngDecoderSlices
{
public:
  DngDecoderSlices(FileMap* file, RawImage img );
  ~DngDecoderSlices(void);
  void addSlice(DngSliceElement slice);
  void startDecoding();
  void decodeSlice(DngDecoderThread* t);
  queue<DngSliceElement> slices;
  vector<DngDecoderThread*> threads;
  FileMap *mFile; 
  RawImage mRaw;
  guint decodedSlices;
  vector<const char*> errors;
  gboolean mFixLjpeg;
};

