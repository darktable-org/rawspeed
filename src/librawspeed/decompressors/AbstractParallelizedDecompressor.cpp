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
#include <vector>                         // for vector

namespace rawspeed {

#ifdef HAVE_PTHREAD
void AbstractParallelizedDecompressor::startThreading() const {
  assert(getThreadCount() > 1);
  const uint32 threadNum =
      std::min(static_cast<uint32>(mRaw->dim.y), getThreadCount());
  assert(threadNum > 1);

  const int y_per_thread = roundUpDivision(mRaw->dim.y, threadNum);
  assert(threadNum * y_per_thread >= (uint32)mRaw->dim.y);
  assert((threadNum - 1) * y_per_thread < (uint32)mRaw->dim.y);

  std::vector<RawDecompressorThread> threads(
      threadNum, RawDecompressorThread(this, threadNum));

  /* Initialize and set thread detached attribute */
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  uint32 i = 0;
  bool fail = false;
  int y_offset = 0;
  for (auto& t : threads) {
    t.taskNo = i;

    t.start_y = y_offset;
    t.end_y = t.start_y + y_per_thread;

    t.end_y = std::min(t.end_y, uint32(mRaw->dim.y));

    if (pthread_create(&t.threadid, &attr, RawDecompressorThread::start_routine,
                       &t) != 0) {
      // If a failure occurs, we need to wait for the already created threads to
      // finish
      fail = true;
      while (threads.size() > i)
        threads.pop_back();
      break;
    }

    y_offset = t.end_y;
    i++;
  }

  for (auto& t : threads)
    pthread_join(t.threadid, nullptr);

  pthread_attr_destroy(&attr);

  if (fail)
    ThrowRDE("Unable to start threads");
}
#endif

} // namespace rawspeed
