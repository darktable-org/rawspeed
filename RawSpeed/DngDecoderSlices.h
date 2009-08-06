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
  int size();
  queue<DngSliceElement> slices;
  vector<DngDecoderThread*> threads;
  FileMap *mFile; 
  RawImage mRaw;
  vector<const char*> errors;
  pthread_mutex_t errMutex;   // Mutex for above
  gboolean mFixLjpeg;
  guint nThreads;
};

