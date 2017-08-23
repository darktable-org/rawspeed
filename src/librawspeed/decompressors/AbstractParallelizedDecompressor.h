/*
    RawSpeed - RAW file decoder.

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

#pragma once

#include "rawspeedconfig.h"                     // for HAVE_PTHREAD
#include "common/Common.h"                      // for uint32, BitOrder
#include "common/RawImage.h"                    // for RawImage
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor

#ifdef HAVE_PTHREAD
#include <pthread.h> // for pthread_t
#endif

namespace rawspeed {

class RawDecompressorThread;

class AbstractParallelizedDecompressor : AbstractDecompressor {
#ifdef HAVE_PTHREAD
  friend class RawDecompressorThread;
  virtual void decompressThreaded(const RawDecompressorThread* t) const = 0;
#endif

public:
  explicit AbstractParallelizedDecompressor(const RawImage& img) : mRaw(img) {}
  virtual ~AbstractParallelizedDecompressor() = default;

protected:
  RawImage mRaw;

#ifdef HAVE_PTHREAD
  virtual void startThreading() const final;
#endif
};

#ifdef HAVE_PTHREAD
class RawDecompressorThread final {
  const AbstractParallelizedDecompressor* const parent;

public:
  RawDecompressorThread(const AbstractParallelizedDecompressor* parent_,
                        uint32 tasksTotal_)
      : parent(parent_), tasksTotal(tasksTotal_) {}

  static void* start_routine(void* arg) {
    const auto* this_ = static_cast<const RawDecompressorThread*>(arg);
    this_->parent->decompressThreaded(this_);
    return nullptr;
  }

  uint32 taskNo = -1;
  const uint32 tasksTotal;

  uint32 start_y = 0;
  uint32 end_y = 0;

  pthread_t threadid;
};
#endif

} // namespace rawspeed
