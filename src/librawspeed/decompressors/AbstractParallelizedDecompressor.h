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
#include "decoders/RawDecoderException.h"       // for RawDecoderException
#include "decompressors/AbstractDecompressor.h" // for AbstractDecompressor
#include "io/IOException.h"                     // for IOException

#ifdef HAVE_PTHREAD
#include <pthread.h> // for pthread_t
#endif

namespace rawspeed {

class RawDecompressorThread;

class AbstractParallelizedDecompressor : public AbstractDecompressor {
  friend class RawDecompressorThread;
  virtual void decompressThreaded(const RawDecompressorThread* t) const = 0;

  void decompressOne(uint32 pieces) const;

public:
  explicit AbstractParallelizedDecompressor(const RawImage& img) : mRaw(img) {}
  virtual ~AbstractParallelizedDecompressor() = default;

  virtual void decompress() const;

protected:
  RawImage mRaw;

  void startThreading(uint32 pieces) const;
};

class RawDecompressorThread final {
  const AbstractParallelizedDecompressor* const parent;

public:
  RawDecompressorThread(const AbstractParallelizedDecompressor* parent_,
                        uint32 tasksTotal_)
      : parent(parent_), tasksTotal(tasksTotal_) {}

  static void* start_routine(void* arg) noexcept {
    const auto* this_ = static_cast<const RawDecompressorThread*>(arg);
    try {
      this_->parent->decompressThreaded(this_);
    } catch (RawDecoderException& err) {
      this_->parent->mRaw->setError(err.what());
    } catch (IOException& err) {
      this_->parent->mRaw->setError(err.what());
    }
    return nullptr;
  }

  uint32 taskNo = ~0U;
  const uint32 tasksTotal;

  uint32 start = 0;
  uint32 end = 0;

#ifdef HAVE_PTHREAD
  pthread_t threadid;
#endif
};

} // namespace rawspeed
