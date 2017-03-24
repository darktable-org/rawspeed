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

#include "rawspeedconfig.h" // for HAVE_PTHREAD
#include "decoders/RawDecoder.h"
#include "common/Common.h"                          // for uint32, getThrea...
#include "common/Point.h"                           // for iPoint2D, iRecta...
#include "decoders/RawDecoderException.h"           // for ThrowRDE, RawDec...
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer
#include "io/FileIOException.h"                     // for FileIOException
#include "io/IOException.h"                         // for IOException
#include "metadata/BlackArea.h"                     // for BlackArea
#include "metadata/Camera.h"                        // for Camera, Hints
#include "metadata/CameraMetaData.h"                // for CameraMetaData
#include "metadata/CameraSensorInfo.h"              // for CameraSensorInfo
#include "metadata/ColorFilterArray.h"              // for ColorFilterArray
#include "parsers/TiffParserException.h"            // for TiffParserException
#include "tiff/TiffEntry.h"                         // for TiffEntry
#include "tiff/TiffIFD.h"                           // for TiffIFD
#include "tiff/TiffTag.h"                           // for TiffTag::STRIPOF...
#include <algorithm>                                // for min
#include <memory>                                   // for allocator_traits...
#include <string>                                   // for string, basic_st...
#include <vector>                                   // for vector

using namespace std;

namespace RawSpeed {

RawDecoder::RawDecoder(Buffer* file) : mRaw(RawImage::create()), mFile(file) {
  failOnUnknown = false;
  interpolateBadPixels = true;
  applyStage1DngOpcodes = true;
  applyCrop = true;
  uncorrectedRawValues = false;
  fujiRotate = true;
}

void RawDecoder::decodeUncompressed(const TiffIFD *rawIFD, BitOrder order) {
  uint32 nslices = rawIFD->getEntry(STRIPOFFSETS)->count;
  TiffEntry *offsets = rawIFD->getEntry(STRIPOFFSETS);
  TiffEntry *counts = rawIFD->getEntry(STRIPBYTECOUNTS);
  uint32 yPerSlice = rawIFD->getEntry(ROWSPERSTRIP)->getU32();
  uint32 width = rawIFD->getEntry(IMAGEWIDTH)->getU32();
  uint32 height = rawIFD->getEntry(IMAGELENGTH)->getU32();
  uint32 bitPerPixel = rawIFD->getEntry(BITSPERSAMPLE)->getU32();

  vector<RawSlice> slices;
  uint32 offY = 0;

  for (uint32 s = 0; s < nslices; s++) {
    RawSlice slice;
    slice.offset = offsets->getU32(s);
    slice.count = counts->getU32(s);
    if (offY + yPerSlice > height)
      slice.h = height - offY;
    else
      slice.h = yPerSlice;

    offY += yPerSlice;

    if (mFile->isValid(slice.offset, slice.count)) // Only decode if size is valid
      slices.push_back(slice);
  }

  if (slices.empty())
    ThrowRDE("No valid slices found. File probably truncated.");

  mRaw->dim = iPoint2D(width, offY);
  mRaw->createData();
  mRaw->whitePoint = (1<<bitPerPixel)-1;

  offY = 0;
  for (uint32 i = 0; i < slices.size(); i++) {
    RawSlice slice = slices[i];
    UncompressedDecompressor u(*mFile, slice.offset, slice.count, mRaw);
    iPoint2D size(width, slice.h);
    iPoint2D pos(0, offY);
    bitPerPixel = (int)((uint64)((uint64)slice.count * 8u) / (slice.h * width));
    try {
      u.readUncompressedRaw(size, pos, width * bitPerPixel / 8, bitPerPixel,
                            order);
    } catch (RawDecoderException &e) {
      if (i>0)
        mRaw->setError(e.what());
      else
        throw;
    } catch (IOException &e) {
      if (i>0)
        mRaw->setError(e.what());
      else {
        ThrowRDE("IO error occurred in first slice, unable to decode more. "
                 "Error is: %s",
                 e.what());
      }
    }
    offY += slice.h;
  }
}

void RawDecoder::askForSamples(const CameraMetaData* meta, const string& make,
                               const string& model, const string& mode) const {
  if ("dng" == mode)
    return;

  writeLog(DEBUG_PRIO_WARNING,
           "Unable to find camera in database: '%s' '%s' "
           "'%s'\nPlease consider providing samples on "
           "<https://raw.pixls.us/>, thanks!",
           make.c_str(), model.c_str(), mode.c_str());
}

bool RawDecoder::checkCameraSupported(const CameraMetaData* meta,
                                      const string& make, const string& model,
                                      const string& mode) {
  mRaw->metadata.make = make;
  mRaw->metadata.model = model;
  const Camera* cam = meta->getCamera(make, model, mode);
  if (!cam) {
    askForSamples(meta, make, model, mode);

    if (failOnUnknown)
      ThrowRDE("Camera '%s' '%s', mode '%s' not supported, and not allowed to guess. Sorry.", make.c_str(), model.c_str(), mode.c_str());

    // Assume the camera can be decoded, but return false, so decoders can see that we are unsure.
    return false;
  }

  if (!cam->supported)
    ThrowRDE("Camera not supported (explicit). Sorry.");

  if (cam->decoderVersion > getDecoderVersion())
    ThrowRDE("Camera not supported in this version. Update RawSpeed for support.");

  hints = cam->hints;
  return true;
}

void RawDecoder::setMetaData(const CameraMetaData* meta, const string& make,
                             const string& model, const string& mode,
                             int iso_speed) {
  mRaw->metadata.isoSpeed = iso_speed;
  const Camera* cam = meta->getCamera(make, model, mode);
  if (!cam) {
    askForSamples(meta, make, model, mode);

    if (failOnUnknown)
      ThrowRDE("Camera '%s' '%s', mode '%s' not supported, and not allowed to guess. Sorry.", make.c_str(), model.c_str(), mode.c_str());

    return;
  }

  mRaw->cfa = cam->cfa;
  mRaw->metadata.canonical_make = cam->canonical_make;
  mRaw->metadata.canonical_model = cam->canonical_model;
  mRaw->metadata.canonical_alias = cam->canonical_alias;
  mRaw->metadata.canonical_id = cam->canonical_id;
  mRaw->metadata.make = make;
  mRaw->metadata.model = model;
  mRaw->metadata.mode = mode;

  if (applyCrop) {
    iPoint2D new_size = cam->cropSize;

    // If crop size is negative, use relative cropping
    if (new_size.x <= 0)
      new_size.x = mRaw->dim.x - cam->cropPos.x + new_size.x;

    if (new_size.y <= 0)
      new_size.y = mRaw->dim.y - cam->cropPos.y + new_size.y;

    mRaw->subFrame(iRectangle2D(cam->cropPos, new_size));
  }

  const CameraSensorInfo *sensor = cam->getSensorInfo(iso_speed);
  mRaw->blackLevel = sensor->mBlackLevel;
  mRaw->whitePoint = sensor->mWhiteLevel;
  mRaw->blackAreas = cam->blackAreas;
  if (mRaw->blackAreas.empty() && !sensor->mBlackLevelSeparate.empty()) {
    auto cfaArea = mRaw->cfa.getSize().area();
    if (mRaw->isCFA && cfaArea <= sensor->mBlackLevelSeparate.size()) {
      for (uint32 i = 0; i < cfaArea; i++) {
        mRaw->blackLevelSeparate[i] = sensor->mBlackLevelSeparate[i];
      }
    } else if (!mRaw->isCFA && mRaw->getCpp() <= sensor->mBlackLevelSeparate.size()) {
      for (uint32 i = 0; i < mRaw->getCpp(); i++) {
        mRaw->blackLevelSeparate[i] = sensor->mBlackLevelSeparate[i];
      }
    }
  }

  // Allow overriding individual blacklevels. Values are in CFA order
  // (the same order as the in the CFA tag)
  // A hint could be:
  // <Hint name="override_cfa_black" value="10,20,30,20"/>
  string cfa_black = hints.get("override_cfa_black", string());
  if (!cfa_black.empty()) {
    vector<string> v = splitString(cfa_black, ',');
    if (v.size() != 4) {
      mRaw->setError("Expected 4 values '10,20,30,20' as values for override_cfa_black hint.");
    } else {
      for (int i = 0; i < 4; i++) {
        mRaw->blackLevelSeparate[i] = stoi(v[i]);
      }
    }
  }
}


void *RawDecoderDecodeThread(void *_this) {
  auto *me = (RawDecoderThread *)_this;
  try {
     me->parent->decodeThreaded(me);
  } catch (RawDecoderException &ex) {
    me->parent->mRaw->setError(ex.what());
  } catch (IOException &ex) {
    me->parent->mRaw->setError(ex.what());
  }
  return nullptr;
}

void RawDecoder::startThreads() {
#ifndef HAVE_PTHREAD
  uint32 threads = 1;
  RawDecoderThread t(this);
  t.start_y = 0;
  t.end_y = mRaw->dim.y;
  RawDecoderDecodeThread(&t);
#else
  uint32 threads;
  bool fail = false;
  threads = min((unsigned)mRaw->dim.y, getThreadCount());
  int y_offset = 0;
  int y_per_thread = (mRaw->dim.y + threads - 1) / threads;

  vector<RawDecoderThread> t(threads, RawDecoderThread(this));

  /* Initialize and set thread detached attribute */
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  for (uint32 i = 0; i < threads; i++) {
    t[i].start_y = y_offset;
    t[i].end_y = min(y_offset + y_per_thread, mRaw->dim.y);
    if (pthread_create(&t[i].threadid, &attr, RawDecoderDecodeThread, &t[i]) != 0) {
      // If a failure occurs, we need to wait for the already created threads to finish
      threads = i-1;
      fail = true;
    }
    y_offset = t[i].end_y;
  }

  for (uint32 i = 0; i < threads; i++) {
    pthread_join(t[i].threadid, nullptr);
  }
  pthread_attr_destroy(&attr);

  if (fail) {
    ThrowRDE("Unable to start threads");
  }
#endif

  if (mRaw->errors.size() >= threads)
    ThrowRDE("All threads reported errors. Cannot load image.");
}

void RawDecoder::decodeThreaded(RawDecoderThread * t) {
  ThrowRDE("This class does not support threaded decoding");
}

RawSpeed::RawImage RawDecoder::decodeRaw()
{
  try {
    RawImage raw = decodeRawInternal();
    raw->metadata.pixelAspectRatio =
        hints.get("pixel_aspect_ratio", raw->metadata.pixelAspectRatio);
    if (interpolateBadPixels)
      raw->fixBadPixels();
    return raw;
  } catch (TiffParserException &e) {
    ThrowRDE("%s", e.what());
  } catch (FileIOException &e) {
    ThrowRDE("%s", e.what());
  } catch (IOException &e) {
    ThrowRDE("%s", e.what());
  }
  return RawImage(nullptr);
}

void RawDecoder::decodeMetaData(const CameraMetaData* meta) {
  try {
    return decodeMetaDataInternal(meta);
  } catch (TiffParserException &e) {
    ThrowRDE("%s", e.what());
  } catch (FileIOException &e) {
    ThrowRDE("%s", e.what());
  } catch (IOException &e) {
    ThrowRDE("%s", e.what());
  }
}

void RawDecoder::checkSupport(const CameraMetaData* meta) {
  try {
    return checkSupportInternal(meta);
  } catch (TiffParserException &e) {
    ThrowRDE("%s", e.what());
  } catch (FileIOException &e) {
    ThrowRDE("%s", e.what());
  } catch (IOException &e) {
    ThrowRDE("%s", e.what());
  }
}

void RawDecoder::startTasks( uint32 tasks )
{
  uint32 threads;
  threads = min(tasks, getThreadCount());
  int ctask = 0;
  vector<RawDecoderThread> t(threads, RawDecoderThread(this));

  // We don't need a thread
  if (threads == 1) {
    while ((uint32)ctask < tasks) {
      t[0].taskNo = ctask++;
      RawDecoderDecodeThread(&t[0]);
    }
    return;
  }

#ifdef HAVE_PTHREAD
  pthread_attr_t attr;

  /* Initialize and set thread detached attribute */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  /* TODO: Create a way to re-use threads */
  void *status;
  while ((uint32)ctask < tasks) {
    for (uint32 i = 0; i < threads && (uint32)ctask < tasks; i++) {
      t[i].taskNo = ctask++;
      pthread_create(&t[i].threadid, &attr, RawDecoderDecodeThread, &t[i]);
    }
    for (uint32 i = 0; i < threads; i++) {
      pthread_join(t[i].threadid, &status);
    }
  }

  if (mRaw->errors.size() >= tasks)
    ThrowRDE("All threads reported errors. Cannot load image.");

#else
  ThrowRDE("Unreachable");
#endif
}

} // namespace RawSpeed
