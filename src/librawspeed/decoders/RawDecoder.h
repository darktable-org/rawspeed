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

#pragma once

#include "rawspeedconfig.h"

#include "common/Common.h"    // for uint32, BitOrder
#include "common/RawImage.h"  // for RawImage
#include "metadata/Camera.h"  // for Hints
#include <string>             // for string

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

namespace RawSpeed {

class Buffer;

class CameraMetaData;

class TiffIFD;

class RawDecoder;

/* Class with information delivered to RawDecoder::decodeThreaded() */
class RawDecoderThread
{
  public:
    explicit RawDecoderThread(RawDecoder* parent_) : parent(parent_) {}
    uint32 start_y = 0;
    uint32 end_y = 0;
    const char* error = nullptr;
#ifdef HAVE_PTHREAD
    pthread_t threadid;
#endif
    RawDecoder* parent;
    uint32 taskNo = -1;
};

void* RawDecoderDecodeThread(void* _this);

class RawDecoder
{
public:
  /* Construct decoder instance - Buffer is a filemap of the file to be decoded
   */
  /* The Buffer is not owned by this class, will not be deleted, and must remain
   */
  /* valid while this object exists */
  explicit RawDecoder(Buffer* file);
  virtual ~RawDecoder() = default;

  /* Check if the decoder can decode the image from this camera */
  /* A RawDecoderException will be thrown if the camera isn't supported */
  /* Unknown cameras does NOT generate any specific feedback */
  /* This function must be overridden by actual decoders */
  void checkSupport(const CameraMetaData* meta);

  /* Attempt to decode the image */
  /* A RawDecoderException will be thrown if the image cannot be decoded, */
  /* and there will not be any data in the mRaw image. */
  RawImage decodeRaw();

  /* This will apply metadata information from the camera database, */
  /* such as crop, black+white level, etc. */
  /* This function is expected to use the protected "setMetaData" */
  /* after retrieving make, model and mode if applicate. */
  /* If meta-data is set during load, this function can be empty. */
  /* The image is expected to be cropped after this, but black/whitelevel */
  /* compensation is not expected to be applied to the image */
  void decodeMetaData(const CameraMetaData* meta);

  /* Called function for filters that are capable of doing simple multi-threaded decode */
  /* The delivered class gives information on what part of the image should be decoded. */
  [[noreturn]] virtual void decodeThreaded(RawDecoderThread* t);

  /* Allows access to the root IFD structure */
  /* If image isn't TIFF based NULL will be returned */
  virtual TiffIFD *getRootIFD() { return nullptr; }

  /* The decoded image - undefined if image has not or could not be decoded. */
  /* Remember this is automatically refcounted, so a reference is retained until this class is destroyed */
  RawImage mRaw;

  /* You can set this if you do not want Rawspeed to attempt to decode images, */
  /* where it does not have reliable information about CFA, cropping, black and white point */
  /* It is pretty safe to leave this disabled (default behaviour), but if you do not want to */
  /* support unknown cameras, you can enable this */
  /* DNGs are always attempted to be decoded, so this variable has no effect on DNGs */
  bool failOnUnknown;

  /* Set how to handle bad pixels. */
  /* If you disable this parameter, no bad pixel interpolation will be done */
  bool interpolateBadPixels;

  /* Apply stage 1 DNG opcodes. */
  /* This usually maps out bad pixels, etc */
  bool applyStage1DngOpcodes;

  /* Apply crop - if false uncropped image is delivered */
  bool applyCrop;

  /* This will skip all corrections, and deliver the raw data */
  /* This will skip any compression curves or other things that */
  /* is needed to get the correct values */
  /* Only enable if you are sure that is what you want */
  bool uncorrectedRawValues;

  /* Should Fuji images be rotated? */
  bool fujiRotate;

  /* Retrieve the main RAW chunk */
  /* Returns NULL if unknown */
  virtual Buffer* getCompressedData() { return nullptr; }

protected:
  /* Attempt to decode the image */
  /* A RawDecoderException will be thrown if the image cannot be decoded, */
  /* and there will not be any data in the mRaw image. */
  /* This function must be overridden by actual decoders. */
  virtual RawImage decodeRawInternal() = 0;
  virtual void decodeMetaDataInternal(const CameraMetaData* meta) = 0;
  virtual void checkSupportInternal(const CameraMetaData* meta) = 0;

  /* Helper function for decoders - splits the image vertically and starts of decoder threads */
  /* The function returns when all threads are done */
  /* All errors are silently pushed into the "errors" array.*/
  /* If all threads report an error an exception will be thrown*/
  void startThreads();

  /* Helper function for decoders -  */
  /* The function returns when all tasks are done */
  /* All errors are silently pushed into the "errors" array.*/
  /* If all threads report an error an exception will be thrown*/
  void startTasks(uint32 tasks);

  /* Ask for sample submisson, if makes sense */
  void askForSamples(const CameraMetaData* meta, const std::string& make,
                     const std::string& model, const std::string& mode) const;

  /* Check the camera and mode against the camera database. */
  /* A RawDecoderException will be thrown if the camera isn't supported */
  /* Unknown cameras does NOT generate any errors, but returns false */
  bool checkCameraSupported(const CameraMetaData* meta, const std::string& make,
                            const std::string& model, const std::string& mode);

  /* Helper function for decodeMetaData(), that find the camera in the CameraMetaData DB */
  /* and sets common settings such as crop, black- white level, and sets CFA information */
  virtual void setMetaData(const CameraMetaData* meta, const std::string& make,
                           const std::string& model, const std::string& mode,
                           int iso_speed = 0);

  /* Generic decompressor for uncompressed images */
  /* order: Order of the bits - see Common.h for possibilities. */
  void decodeUncompressed(const TiffIFD* rawIFD, BitOrder order);

  /* The Raw input file to be decoded */
  Buffer* mFile;

  /* Decoder version */
  /* This can be used to avoid newer version of an xml file to indicate that a file */
  /* can be decoded, when a specific version of the code is needed */
  /* Higher number in camera xml file: Files for this camera will not be decoded */
  /* Higher number in code than xml: Image will be decoded. */
  virtual int getDecoderVersion() const = 0;

  /* Hints set for the camera after checkCameraSupported has been called from the implementation*/
  Hints hints;

  struct RawSlice;
};

struct RawDecoder::RawSlice {
  uint32 h = 0;
  uint32 offset = 0;
  uint32 count = 0;
};

} // namespace RawSpeed
