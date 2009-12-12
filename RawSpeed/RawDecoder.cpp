#include "StdAfx.h"
#include "RawDecoder.h"
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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

    http://www.klauspost.com
*/

namespace RawSpeed {

RawDecoder::RawDecoder(FileMap* file) : mFile(file), mRaw(RawImage::create()) {
  decoderVersion = 0;
}

RawDecoder::~RawDecoder(void) {
  for (guint i = 0 ; i < errors.size(); i++) {
    free((void*)errors[i]);
  }
  errors.clear();
}

void RawDecoder::readUncompressedRaw(ByteStream &input, iPoint2D& size, iPoint2D& offset, int inputPitch, int bitPerPixel, gboolean MSBOrder) {
  guchar* data = mRaw->getData();
  guint outPitch = mRaw->pitch;
  guint w = size.x;
  guint h = size.y;
  guint cpp = mRaw->getCpp();

  if (input.getRemainSize() < (inputPitch*h)) {
    h = input.getRemainSize() / inputPitch - 1;
  }
  if (bitPerPixel > 16)
    ThrowRDE("readUncompressedRaw: Unsupported bit depth");

  guint skipBits = inputPitch - w * bitPerPixel / 8;  // Skip per line
  if (offset.y > mRaw->dim.y)
    ThrowRDE("readUncompressedRaw: Invalid y offset");
  if (offset.x + size.x > mRaw->dim.x)
    ThrowRDE("readUncompressedRaw: Invalid x offset");

  guint y = offset.y;
  h = MIN(h + (guint)offset.y, (guint)mRaw->dim.y);

  if (MSBOrder) {
    BitPumpMSB bits(&input);
    w *= cpp;
    for (; y < h; y++) {
      gushort* dest = (gushort*) & data[offset.x*sizeof(gushort)*cpp+y*outPitch];
      for (guint x = 0 ; x < w; x++) {
        guint b = bits.getBits(bitPerPixel);
        dest[x] = b;
      }
      bits.skipBits(skipBits);
    }

  } else {

    if (bitPerPixel == 16)  {
      BitBlt(&data[offset.x*sizeof(gushort)*cpp+y*outPitch], outPitch,
             input.getData(), inputPitch, w*mRaw->bpp, h - y);
      return;
    }
    BitPumpPlain bits(&input);
    w *= cpp;
    for (; y < h; y++) {
      gushort* dest = (gushort*) & data[offset.x*sizeof(gushort)+y*outPitch];
      for (guint x = 0 ; x < w; x++) {
        guint b = bits.getBits(bitPerPixel);
        dest[x] = b;
      }
      bits.skipBits(skipBits);
    }
  }
}

void RawDecoder::Decode12BitRaw(ByteStream &input, guint w, guint h) {
  guchar* data = mRaw->getData();
  guint pitch = mRaw->pitch;
  const guchar *in = input.getData();
  if (input.getRemainSize() < (w*h*3 / 2)) {
    h = input.getRemainSize() / (w * 3 / 2) - 1;
  }
  for (guint y = 0; y < h; y++) {
    gushort* dest = (gushort*) & data[y*pitch];
    for (guint x = 0 ; x < w; x += 2) {
      guint g1 = *in++;
      guint g2 = *in++;
      dest[x] = g1 | ((g2 & 0xf) << 8);
      guint g3 = *in++;
      dest[x+1] = (g2 >> 4) | (g3 << 4);
    }
  }
}

void RawDecoder::checkCameraSupported(CameraMetaData *meta, string make, string model, string mode) {
  TrimSpaces(make);
  TrimSpaces(model);
  Camera* cam = meta->getCamera(make, model, mode);
  if (!cam) {
    if (mode.length() == 0)
      printf("Unable to find camera in database: %s %s %s\n", make.c_str(), model.c_str(), mode.c_str());

    return;    // Assume true.
  }

  if (!cam->supported)
    ThrowRDE("Camera not supported (explicit). Sorry.");

  if (cam->decoderVersion > decoderVersion)
    ThrowRDE("Camera not supported in this version. Update RawSpeed for support.");
}

void RawDecoder::setMetaData(CameraMetaData *meta, string make, string model, string mode) {
  TrimSpaces(make);
  TrimSpaces(model);
  Camera *cam = meta->getCamera(make, model, mode);
  if (!cam) {
    printf("Unable to find camera in database: %s %s %s\nPlease upload file to ftp.rawstudio.org, thanks!\n", make.c_str(), model.c_str(), mode.c_str());
    return;
  }

  iPoint2D new_size = cam->cropSize;

  // If crop size is negative, use relative cropping
  if (new_size.x <= 0)
    new_size.x = mRaw->dim.x - cam->cropPos.x + new_size.x;

  if (new_size.y <= 0)
    new_size.y = mRaw->dim.y - cam->cropPos.y + new_size.y;

  mRaw->subFrame(cam->cropPos, new_size);
  mRaw->cfa = cam->cfa;

  // Shift CFA to match crop
  if (cam->cropPos.x & 1)
    mRaw->cfa.shiftLeft();
  if (cam->cropPos.y & 1)
    mRaw->cfa.shiftDown();

  mRaw->blackLevel = cam->black;
  mRaw->whitePoint = cam->white;
}

void RawDecoder::TrimSpaces(string& str) {
  // Trim Both leading and trailing spaces
  size_t startpos = str.find_first_not_of(" \t"); // Find the first character position after excluding leading blank spaces
  size_t endpos = str.find_last_not_of(" \t"); // Find the first character position from reverse af

  // if all spaces or empty return an empty string
  if ((string::npos == startpos) || (string::npos == endpos)) {
    str = "";
  } else
    str = str.substr(startpos, endpos - startpos + 1);
}


void *RawDecoderDecodeThread(void *_this) {
  RawDecoderThread* me = (RawDecoderThread*)_this;
  try {
    me->parent->decodeThreaded(me);
  } catch (RawDecoderException ex) {
    me->error = _strdup(ex.what());
  }
  pthread_exit(NULL);
  return 0;
}

void RawDecoder::startThreads() {
  guint threads;
#ifdef WIN32
  threads = pthread_num_processors_np();
#else
  // FIXME: Don't depend on Rawstudio.
  threads = rs_get_number_of_processor_cores(); 
#endif
  int y_offset = 0;
  int y_per_thread = (mRaw->dim.y + threads - 1) / threads;
  RawDecoderThread *t = new RawDecoderThread[threads];

  pthread_attr_t attr;

  /* Initialize and set thread detached attribute */
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  for (guint i = 0; i < threads; i++) {
    t[i].start_y = y_offset;
    t[i].end_y = MIN(y_offset + y_per_thread, mRaw->dim.y);
    t[i].parent = this;
    pthread_create(&t[i].threadid, &attr, RawDecoderDecodeThread, &t[i]);
    y_offset = t[i].end_y;
  }

  void *status;
  for (guint i = 0; i < threads; i++) {
    pthread_join(t[i].threadid, &status);
    if (t[i].error) {
      errors.push_back(t[i].error);
    }
  }
  delete[] t;
}

void RawDecoder::decodeThreaded(RawDecoderThread * t) {
  ThrowRDE("Internal Error: This class does not support threaded decoding");
}

} // namespace RawSpeed