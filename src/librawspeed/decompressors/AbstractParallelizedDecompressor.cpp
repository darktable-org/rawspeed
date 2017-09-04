/*
    RawSpeed - RAW file Decompressor.

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

#include "rawspeedconfig.h"                                 // for HAVE_PTHREAD
#include "decompressors/AbstractParallelizedDecompressor.h" // for AbstractPar..
#include "common/Point.h"                                   // for iPoint2D
#include "decoders/RawDecoderException.h" // for ThrowRDE, RawDec...
#include <algorithm>                      // for min
#include <cassert>                        // for assert
#include <numeric>                        // for accumulate
#include <vector>                         // for vector

namespace rawspeed {

#ifdef HAVE_PTHREAD
std::vector<uint32>
AbstractParallelizedDecompressor::piecesPerThread(uint32 threads,
                                                  uint32 pieces) {
  std::vector<uint32> buckets;
  buckets.resize(threads, 0);

  // split all the pieces between all the threads 'evenly'
  int piecesLeft = pieces;
  while (piecesLeft > 0) {
    for (auto& bucket : buckets) {
      --piecesLeft;
      ++bucket;
      if (0 == piecesLeft)
        break;
    }
  }
  assert(piecesLeft == 0);
  assert(std::accumulate(buckets.begin(), buckets.end(), 0UL) == pieces);

  return buckets;
}
#endif

#ifdef HAVE_PTHREAD
void AbstractParallelizedDecompressor::startThreading(uint32 pieces) const {
  assert(getThreadCount() > 1);

  const uint32 threadNum =
      std::min(static_cast<uint32>(pieces), getThreadCount());
  assert(threadNum > 1);

  const std::vector<uint32> buckets = piecesPerThread(threadNum, pieces);

  std::vector<RawDecompressorThread> threads(
      threadNum, RawDecompressorThread(this, threadNum));

  /* Initialize and set thread detached attribute */
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  uint32 i = 0;
  bool fail = false;
  int offset = 0;
  for (auto& t : threads) {
    t.taskNo = i;

    t.start = offset;
    t.end = t.start + buckets[i];

    assert(t.start < pieces);
    assert(t.end > 0);
    assert(t.end > t.start);
    assert(t.end <= pieces);

#ifndef NDEBUG
    if (i > 0) {
      assert(t.start > threads[i - 1].start);
      assert(t.end > threads[i - 1].end);
      assert(t.start == threads[i - 1].end);
    }
#endif

    if (pthread_create(&t.threadid, &attr, RawDecompressorThread::start_routine,
                       &t) != 0) {
      // If a failure occurs, we need to wait for the already created threads to
      // finish
      fail = true;
      while (threads.size() > i)
        threads.pop_back();
      break;
    }

    offset = t.end;
    i++;
  }

  for (auto& t : threads)
    pthread_join(t.threadid, nullptr);

  pthread_attr_destroy(&attr);

  if (fail)
    ThrowRDE("Unable to start threads");
}
#else
void AbstractParallelizedDecompressor::startThreading(uint32 pieces) const {
  RawDecompressorThread t(this, 1);
  t.taskNo = 0;
  t.start = 0;
  t.end = pieces;

  RawDecompressorThread::start_routine(&t);
}
#endif

void AbstractParallelizedDecompressor::decode() const {
  startThreading(mRaw->dim.y);
}

} // namespace rawspeed
