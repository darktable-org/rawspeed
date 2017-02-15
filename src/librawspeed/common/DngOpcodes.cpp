
/*
RawSpeed - RAW file decoder.

Copyright (C) 2012 Klaus Post

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

#include "common/DngOpcodes.h"
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/Endianness.h"
#include "tiff/TiffEntry.h" // for TiffEntry
#include <algorithm>        // for min
#include <cmath>            // for lround, pow
#include <cstddef>          // for NULL, size_t

using namespace std;

namespace RawSpeed {

DngOpcodes::DngOpcodes(TiffEntry *entry)
{
  host = getHostEndianness();
  const uchar8* data = entry->getData(entry->count);
  uint32 entry_size = entry->count;

  if (entry_size < 20)
    ThrowRDE("DngOpcodes: Not enough bytes to read a single opcode");

  uint32 opcode_count = getULong(&data[0]);
  int bytes_used = 4;
  for (uint32 i = 0; i < opcode_count; i++) {
    if ((int)entry_size - bytes_used < 16)
      ThrowRDE("DngOpcodes: Not enough bytes to read a new opcode");

    uint32 code = getULong(&data[bytes_used]);
    //uint32 version = getULong(&data[bytes_used+4]);
    uint32 flags = getULong(&data[bytes_used+8]);
    uint32 expected_size = getULong(&data[bytes_used+12]);
    bytes_used += 16;
    uint32 opcode_used = 0;
    switch (code)
    {
      case 4:
        mOpcodes.push_back(new OpcodeFixBadPixelsConstant(&data[bytes_used], entry_size - bytes_used, &opcode_used));
        break;
      case 5:
        mOpcodes.push_back(new OpcodeFixBadPixelsList(&data[bytes_used], entry_size - bytes_used, &opcode_used));
        break;
      case 6:
        mOpcodes.push_back(new OpcodeTrimBounds(&data[bytes_used], entry_size - bytes_used, &opcode_used));
        break;
      case 7:
        mOpcodes.push_back(new OpcodeMapTable(&data[bytes_used], entry_size - bytes_used, &opcode_used));
        break;
      case 8:
        mOpcodes.push_back(new OpcodeMapPolynomial(&data[bytes_used], entry_size - bytes_used, &opcode_used));
        break;
      case 10:
        mOpcodes.push_back(new OpcodeDeltaPerRow(&data[bytes_used], entry_size - bytes_used, &opcode_used));
        break;
      case 11:
        mOpcodes.push_back(new OpcodeDeltaPerCol(&data[bytes_used], entry_size - bytes_used, &opcode_used));
        break;
      case 12:
        mOpcodes.push_back(new OpcodeScalePerRow(&data[bytes_used], entry_size - bytes_used, &opcode_used));
        break;
      case 13:
        mOpcodes.push_back(new OpcodeScalePerCol(&data[bytes_used], entry_size - bytes_used, &opcode_used));
        break;
      default:
        // Throw Error if not marked as optional
        if (!(flags & 1))
          ThrowRDE("DngOpcodes: Unsupported Opcode: %d", code);
    }
    if (opcode_used != expected_size)
      ThrowRDE("DngOpcodes: Inconsistent length of opcode");
    bytes_used += opcode_used;
  }
}

DngOpcodes::~DngOpcodes() {
  size_t codes = mOpcodes.size();
  for (uint32 i = 0; i < codes; i++)
    delete mOpcodes[i];
  mOpcodes.clear();
}

/* TODO: Apply in separate threads */
RawImage& DngOpcodes::applyOpCodes( RawImage &img )
{
  size_t codes = mOpcodes.size();
  for (uint32 i = 0; i < codes; i++)
  {
    DngOpcode* code = mOpcodes[i];
    RawImage img_out = code->createOutput(img);
    iRectangle2D fullImage(0,0,img->dim.x, img->dim.y);

    if (!code->mAoi.isThisInside(fullImage))
      ThrowRDE("DngOpcodes: Area of interest not inside image!");
    if (code->mAoi.hasPositiveArea()) {
      code->apply(img, img_out, code->mAoi.getTop(), code->mAoi.getBottom());
      img = img_out;
    }
  }
  return img;
}

/***************** OpcodeFixBadPixelsConstant   ****************/

OpcodeFixBadPixelsConstant::OpcodeFixBadPixelsConstant(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used )
{
  if (param_max_bytes < 8)
    ThrowRDE("OpcodeFixBadPixelsConstant: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  mValue = getLong(&parameters[0]);
  // Bayer Phase not used
  *bytes_used = 8;
  mFlags = MultiThreaded;
}

RawImage& OpcodeFixBadPixelsConstant::createOutput( RawImage &in )
{
  // These limitations are present within the DNG SDK as well.
  if (in->getDataType() != TYPE_USHORT16)
    ThrowRDE("OpcodeFixBadPixelsConstant: Only 16 bit images supported");

  if (in->getCpp() > 1)
    ThrowRDE("OpcodeFixBadPixelsConstant: This operation is only supported with 1 component");

  return in;
}

void OpcodeFixBadPixelsConstant::apply( RawImage &in, RawImage &out, uint32 startY, uint32 endY )
{
  iPoint2D crop = in->getCropOffset();
  uint32 offset = crop.x | (crop.y << 16);
  vector<uint32> bad_pos;
  for (uint32 y = startY; y < endY; y++) {
    auto *src = (ushort16 *)out->getData(0, y);
    for (uint32 x = 0; x < (uint32)in->dim.x; x++) {
      if (src[x]== mValue) {
        bad_pos.push_back(offset + ((uint32)x | (uint32)y<<16));
      }
    }
  }
  if (!bad_pos.empty())
    out->mBadPixelPositions.append(bad_pos);
}

/***************** OpcodeFixBadPixelsList   ****************/

OpcodeFixBadPixelsList::OpcodeFixBadPixelsList( const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used )
{
  if (param_max_bytes < 12)
    ThrowRDE("OpcodeFixBadPixelsList: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  // Skip phase - we don't care
  uint64 BadPointCount = getULong(&parameters[4]);
  uint64 BadRectCount = getULong(&parameters[8]);
  bytes_used[0] = 12;

  if (12 + BadPointCount * 8 + BadRectCount * 16 > (uint64) param_max_bytes)
    ThrowRDE("OpcodeFixBadPixelsList: Ran out parameter space, only %u bytes left.", param_max_bytes);

  // Read points
  for (uint64 i = 0; i < BadPointCount; i++) {
    auto BadPointRow = (uint32)getLong(&parameters[bytes_used[0]]);
    auto BadPointCol = (uint32)getLong(&parameters[bytes_used[0] + 4]);
    bytes_used[0] += 8;
    bad_pos.push_back(BadPointRow | (BadPointCol << 16));
  }

  // Read rects
  for (uint64 i = 0; i < BadRectCount; i++) {
    auto BadRectTop = (uint32)getLong(&parameters[bytes_used[0]]);
    auto BadRectLeft = (uint32)getLong(&parameters[bytes_used[0] + 4]);
    auto BadRectBottom = (uint32)getLong(&parameters[bytes_used[0]]);
    auto BadRectRight = (uint32)getLong(&parameters[bytes_used[0] + 4]);
    bytes_used[0] += 16;
    if (BadRectTop < BadRectBottom && BadRectLeft < BadRectRight) {
      for (uint32 y = BadRectLeft; y <= BadRectRight; y++) {
        for (uint32 x = BadRectTop; x <= BadRectBottom; x++) {
          bad_pos.push_back(x | (y << 16));
        }
      }
    }
  }
}

void OpcodeFixBadPixelsList::apply( RawImage &in, RawImage &out, uint32 startY, uint32 endY )
{
  iPoint2D crop = in->getCropOffset();
  uint32 offset = crop.x | (crop.y << 16);
  for (unsigned int &bad_po : bad_pos) {
    uint32 pos = offset + bad_po;
    out->mBadPixelPositions.push_back(pos);
  }
}

 /***************** OpcodeTrimBounds   ****************/

OpcodeTrimBounds::OpcodeTrimBounds(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used )
{
  if (param_max_bytes < 16)
    ThrowRDE("OpcodeTrimBounds: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  mTop = getLong(&parameters[0]);
  mLeft = getLong(&parameters[4]);
  mBottom = getLong(&parameters[8]);
  mRight = getLong(&parameters[12]);
  *bytes_used = 16;
}

void OpcodeTrimBounds::apply( RawImage &in, RawImage &out, uint32 startY, uint32 endY )
{
  iRectangle2D crop(mLeft, mTop, mRight-mLeft, mBottom-mTop);
  out->subFrame(crop);
}

/***************** OpcodeMapTable   ****************/

OpcodeMapTable::OpcodeMapTable(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used )
{
  if (param_max_bytes < 36)
    ThrowRDE("OpcodeMapTable: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  mAoi.setAbsolute(getLong(&parameters[4]), getLong(&parameters[0]), getLong(&parameters[12]), getLong(&parameters[8]));
  mFirstPlane = getLong(&parameters[16]);
  mPlanes = getLong(&parameters[20]);
  mRowPitch = getLong(&parameters[24]);
  mColPitch = getLong(&parameters[28]);
  if (mPlanes == 0)
    ThrowRDE("OpcodeMapPolynomial: Zero planes");
  if (mRowPitch == 0 || mColPitch == 0)
    ThrowRDE("OpcodeMapPolynomial: Invalid Pitch");

  int tablesize = getLong(&parameters[32]);
  *bytes_used = 36;

  if (tablesize <= 0)
    ThrowRDE("OpcodeMapTable: Table size must be positive");
  if (tablesize > 65536)
    ThrowRDE("OpcodeMapTable: A map with more than 65536 entries not allowed");

  if (param_max_bytes < 36 + ((uint64)tablesize*2))
    ThrowRDE("OpcodeMapPolynomial: Not enough data to read parameters, only %u bytes left.", param_max_bytes);

  for (int i = 0; i <= 65535; i++)
  {
    int location = min(tablesize-1, i);
    mLookup[i] = getUshort(&parameters[36+2*location]);
  }

  *bytes_used += tablesize*2;
  mFlags = MultiThreaded | PureLookup;
}


RawImage& OpcodeMapTable::createOutput( RawImage &in )
{
  if (in->getDataType() != TYPE_USHORT16)
    ThrowRDE("OpcodeMapTable: Only 16 bit images supported");

  if (mFirstPlane > in->getCpp())
    ThrowRDE("OpcodeMapTable: Not that many planes in actual image");

  if (mFirstPlane+mPlanes > in->getCpp())
    ThrowRDE("OpcodeMapTable: Not that many planes in actual image");

  return in;
}

void OpcodeMapTable::apply( RawImage &in, RawImage &out, uint32 startY, uint32 endY )
{
  int cpp = out->getCpp();
  for (uint64 y = startY; y < endY; y += mRowPitch) {
    auto *src = (ushort16 *)out->getData(mAoi.getLeft(), y);
    // Add offset, so this is always first plane
    src+=mFirstPlane;
    for (uint64 x = 0; x < (uint64) mAoi.getWidth(); x += mColPitch) {
      for (uint64 p = 0; p < mPlanes; p++)
      {
        src[x*cpp+p] = mLookup[src[x*cpp+p]];
      }
    }
  }
}



 /***************** OpcodeMapPolynomial   ****************/

OpcodeMapPolynomial::OpcodeMapPolynomial(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used )
{
  if (param_max_bytes < 36)
    ThrowRDE("OpcodeMapPolynomial: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  mAoi.setAbsolute(getLong(&parameters[4]), getLong(&parameters[0]), getLong(&parameters[12]), getLong(&parameters[8]));
  mFirstPlane = getLong(&parameters[16]);
  mPlanes = getLong(&parameters[20]);
  mRowPitch = getLong(&parameters[24]);
  mColPitch = getLong(&parameters[28]);
  if (mPlanes == 0)
    ThrowRDE("OpcodeMapPolynomial: Zero planes");
  if (mRowPitch == 0 || mColPitch == 0)
    ThrowRDE("OpcodeMapPolynomial: Invalid Pitch");

  mDegree = getLong(&parameters[32]);
  *bytes_used = 36;
  if (mDegree > 8)
    ThrowRDE("OpcodeMapPolynomial: A polynomial with more than 8 degrees not allowed");
  if (param_max_bytes < 36 + (mDegree*8))
    ThrowRDE("OpcodeMapPolynomial: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  for (uint64 i = 0; i <= mDegree; i++)
    mCoefficient[i] = getDouble(&parameters[36+8*i]);
  *bytes_used += 8*mDegree+8;
  mFlags = MultiThreaded | PureLookup;
}


RawImage& OpcodeMapPolynomial::createOutput( RawImage &in )
{
  if (in->getDataType() != TYPE_USHORT16)
    ThrowRDE("OpcodeMapPolynomial: Only 16 bit images supported");

  if (mFirstPlane > in->getCpp())
    ThrowRDE("OpcodeMapPolynomial: Not that many planes in actual image");

  if (mFirstPlane+mPlanes > in->getCpp())
    ThrowRDE("OpcodeMapPolynomial: Not that many planes in actual image");

  // Create lookup
  for (int i = 0; i < 65536; i++)
  {
    double in_val = (double)i/65536.0;
    double val = mCoefficient[0];
    for (uint64 j = 1; j <= mDegree; j++)
      val += mCoefficient[j] * pow(in_val, (double)(j));
    mLookup[i] = clampBits((int)(val*65535.5), 16);
  }
  return in;
}

void OpcodeMapPolynomial::apply( RawImage &in, RawImage &out, uint32 startY, uint32 endY )
{
  int cpp = out->getCpp();
  for (uint64 y = startY; y < endY; y += mRowPitch) {
    auto *src = (ushort16 *)out->getData(mAoi.getLeft(), y);
    // Add offset, so this is always first plane
    src+=mFirstPlane;
    for (uint64 x = 0; x < (uint64)mAoi.getWidth(); x += mColPitch) {
      for (uint64 p = 0; p < mPlanes; p++)
      {
        src[x*cpp+p] = mLookup[src[x*cpp+p]];
      }
    }
  }
}

/***************** OpcodeDeltaPerRow   ****************/

OpcodeDeltaPerRow::OpcodeDeltaPerRow(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used )
{
  if (param_max_bytes < 36)
    ThrowRDE("OpcodeDeltaPerRow: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  mAoi.setAbsolute(getLong(&parameters[4]), getLong(&parameters[0]), getLong(&parameters[12]), getLong(&parameters[8]));
  mFirstPlane = getLong(&parameters[16]);
  mPlanes = getLong(&parameters[20]);
  mRowPitch = getLong(&parameters[24]);
  mColPitch = getLong(&parameters[28]);
  if (mPlanes == 0)
    ThrowRDE("OpcodeDeltaPerRow: Zero planes");
  if (mRowPitch == 0 || mColPitch == 0)
    ThrowRDE("OpcodeDeltaPerRow: Invalid Pitch");

  mCount = getLong(&parameters[32]);
  *bytes_used = 36;
  if (param_max_bytes < 36 + (mCount*4))
    ThrowRDE("OpcodeDeltaPerRow: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  if ((uint64)mAoi.getHeight() != mCount)
    ThrowRDE("OpcodeDeltaPerRow: Element count (%llu) does not match height of area (%d.", mCount, mAoi.getHeight());

  for (uint64 i = 0; i < mCount; i++)
    mDelta[i] = getFloat(&parameters[36+4*i]);
  *bytes_used += 4*mCount;
  mFlags = MultiThreaded;
}


RawImage& OpcodeDeltaPerRow::createOutput( RawImage &in )
{
  if (mFirstPlane > in->getCpp())
    ThrowRDE("OpcodeDeltaPerRow: Not that many planes in actual image");

  if (mFirstPlane+mPlanes > in->getCpp())
    ThrowRDE("OpcodeDeltaPerRow: Not that many planes in actual image");

  return in;
}

void OpcodeDeltaPerRow::apply( RawImage &in, RawImage &out, uint32 startY, uint32 endY )
{
  if (in->getDataType() == TYPE_USHORT16) {
    int cpp = out->getCpp();
    for (uint64 y = startY; y < endY; y += mRowPitch) {
      auto *src = (ushort16 *)out->getData(mAoi.getLeft(), y);
      // Add offset, so this is always first plane
      src+=mFirstPlane;
      auto delta = (int)(65535.0f * mDelta[y]);
      for (uint64 x = 0; x < (uint64)mAoi.getWidth(); x += mColPitch) {
        for (uint64 p = 0; p < mPlanes; p++)
        {
          //TODO: the way clampBits is used here, is _very_ likely wrong
          src[x*cpp+p] = clampBits(16,delta + src[x*cpp+p]);
        }
      }
    }
  } else {
    int cpp = out->getCpp();
    for (uint64 y = startY; y < endY; y += mRowPitch) {
      auto *src = (float *)out->getData(mAoi.getLeft(), y);
      // Add offset, so this is always first plane
      src+=mFirstPlane;
      float delta = mDelta[y];
      for (uint64 x = 0; x < (uint64)mAoi.getWidth(); x += mColPitch) {
        for (uint64 p = 0; p < mPlanes; p++)
        {
          src[x*cpp+p] = delta + src[x*cpp+p];
        }
      }
    }
  }
}

/***************** OpcodeDeltaPerCol   ****************/

OpcodeDeltaPerCol::OpcodeDeltaPerCol(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used )
{
  if (param_max_bytes < 36)
    ThrowRDE("OpcodeDeltaPerCol: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  mAoi.setAbsolute(getLong(&parameters[4]), getLong(&parameters[0]), getLong(&parameters[12]), getLong(&parameters[8]));
  mFirstPlane = getLong(&parameters[16]);
  mPlanes = getLong(&parameters[20]);
  mRowPitch = getLong(&parameters[24]);
  mColPitch = getLong(&parameters[28]);
  if (mPlanes == 0)
    ThrowRDE("OpcodeDeltaPerCol: Zero planes");
  if (mRowPitch == 0 || mColPitch == 0)
    ThrowRDE("OpcodeDeltaPerCol: Invalid Pitch");

  mCount = getLong(&parameters[32]);
  *bytes_used = 36;
  if (param_max_bytes < 36 + (mCount*4))
    ThrowRDE("OpcodeDeltaPerCol: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  if ((uint64)mAoi.getWidth() != mCount)
    ThrowRDE("OpcodeDeltaPerRow: Element count (%llu) does not match width of area (%d).", mCount, mAoi.getWidth());

  for (uint64 i = 0; i < mCount; i++)
    mDelta[i] = getFloat(&parameters[36+4*i]);
  *bytes_used += 4*mCount;
  mFlags = MultiThreaded;
  mDeltaX = nullptr;
}

OpcodeDeltaPerCol::~OpcodeDeltaPerCol() {
  if (mDeltaX)
    delete[] mDeltaX;
  mDeltaX = nullptr;
}


RawImage& OpcodeDeltaPerCol::createOutput( RawImage &in )
{
  if (mFirstPlane > in->getCpp())
    ThrowRDE("OpcodeDeltaPerCol: Not that many planes in actual image");

  if (mFirstPlane+mPlanes > in->getCpp())
    ThrowRDE("OpcodeDeltaPerCol: Not that many planes in actual image");

  if (in->getDataType() == TYPE_USHORT16) {
    if (mDeltaX)
      delete[] mDeltaX;
    int w = mAoi.getWidth();
    mDeltaX = new int[w];
    for (int i = 0; i < w; i++)
      mDeltaX[i] = lround(65535.0f * mDelta[i]);
  }
  return in;
}

void OpcodeDeltaPerCol::apply( RawImage &in, RawImage &out, uint32 startY, uint32 endY )
{
  if (in->getDataType() == TYPE_USHORT16) {
    int cpp = out->getCpp();
    for (uint64 y = startY; y < endY; y += mRowPitch) {
      auto *src = (ushort16 *)out->getData(mAoi.getLeft(), y);
      // Add offset, so this is always first plane
      src+=mFirstPlane;
      for (uint64 x = 0; x < (uint64)mAoi.getWidth(); x += mColPitch) {
        for (uint64 p = 0; p < mPlanes; p++)
        {
          src[x*cpp+p] = clampBits(16, mDeltaX[x] + src[x*cpp+p]);
        }
      }
    }
  } else {
    int cpp = out->getCpp();
    for (uint64 y = startY; y < endY; y += mRowPitch) {
      auto *src = (float *)out->getData(mAoi.getLeft(), y);
      // Add offset, so this is always first plane
      src+=mFirstPlane;
      for (uint64 x = 0; x < (uint64)mAoi.getWidth(); x += mColPitch) {
        for (uint64 p = 0; p < mPlanes; p++)
        {
          src[x*cpp+p] = mDelta[x] + src[x*cpp+p];
        }
      }
    }
  }
}

/***************** OpcodeScalePerRow   ****************/

OpcodeScalePerRow::OpcodeScalePerRow(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used )
{
  if (param_max_bytes < 36)
    ThrowRDE("OpcodeScalePerRow: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  mAoi.setAbsolute(getLong(&parameters[4]), getLong(&parameters[0]), getLong(&parameters[12]), getLong(&parameters[8]));
  mFirstPlane = getLong(&parameters[16]);
  mPlanes = getLong(&parameters[20]);
  mRowPitch = getLong(&parameters[24]);
  mColPitch = getLong(&parameters[28]);
  if (mPlanes == 0)
    ThrowRDE("OpcodeScalePerRow: Zero planes");
  if (mRowPitch == 0 || mColPitch == 0)
    ThrowRDE("OpcodeScalePerRow: Invalid Pitch");

  mCount = getLong(&parameters[32]);
  *bytes_used = 36;
  if (param_max_bytes < 36 + (mCount*4))
    ThrowRDE("OpcodeScalePerRow: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  if ((uint64)mAoi.getHeight() != mCount)
    ThrowRDE("OpcodeScalePerRow: Element count (%llu) does not match height of area (%d).", mCount, mAoi.getHeight());

  for (uint64 i = 0; i < mCount; i++)
    mDelta[i] = getFloat(&parameters[36+4*i]);
  *bytes_used += 4*mCount;
  mFlags = MultiThreaded;
}


RawImage& OpcodeScalePerRow::createOutput( RawImage &in )
{
  if (mFirstPlane > in->getCpp())
    ThrowRDE("OpcodeScalePerRow: Not that many planes in actual image");

  if (mFirstPlane+mPlanes > in->getCpp())
    ThrowRDE("OpcodeScalePerRow: Not that many planes in actual image");

  return in;
}

void OpcodeScalePerRow::apply( RawImage &in, RawImage &out, uint32 startY, uint32 endY )
{
  if (in->getDataType() == TYPE_USHORT16) {
    int cpp = out->getCpp();
    for (uint64 y = startY; y < endY; y += mRowPitch) {
      auto *src = (ushort16 *)out->getData(mAoi.getLeft(), y);
      // Add offset, so this is always first plane
      src+=mFirstPlane;
      auto delta = (int)(1024.0f * mDelta[y]);
      for (uint64 x = 0; x < (uint64)mAoi.getWidth(); x += mColPitch) {
        for (uint64 p = 0; p < mPlanes; p++)
        {
          src[x*cpp+p] = clampBits(16,(delta * src[x*cpp+p] + 512) >> 10);
        }
      }
    }
  } else {
    int cpp = out->getCpp();
    for (uint64 y = startY; y < endY; y += mRowPitch) {
      auto *src = (float *)out->getData(mAoi.getLeft(), y);
      // Add offset, so this is always first plane
      src+=mFirstPlane;
      float delta = mDelta[y];
      for (uint64 x = 0; x < (uint64)mAoi.getWidth(); x += mColPitch) {
        for (uint64 p = 0; p < mPlanes; p++)
        {
          src[x*cpp+p] = delta * src[x*cpp+p];
        }
      }
    }
  }
}

/***************** OpcodeScalePerCol   ****************/

OpcodeScalePerCol::OpcodeScalePerCol(const uchar8* parameters, uint32 param_max_bytes, uint32 *bytes_used )
{
  if (param_max_bytes < 36)
    ThrowRDE("OpcodeScalePerCol: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  mAoi.setAbsolute(getLong(&parameters[4]), getLong(&parameters[0]), getLong(&parameters[12]), getLong(&parameters[8]));
  mFirstPlane = getLong(&parameters[16]);
  mPlanes = getLong(&parameters[20]);
  mRowPitch = getLong(&parameters[24]);
  mColPitch = getLong(&parameters[28]);
  if (mPlanes == 0)
    ThrowRDE("OpcodeScalePerCol: Zero planes");
  if (mRowPitch == 0 || mColPitch == 0)
    ThrowRDE("OpcodeScalePerCol: Invalid Pitch");

  mCount = getLong(&parameters[32]);
  *bytes_used = 36;
  if (param_max_bytes < 36 + (mCount*4))
    ThrowRDE("OpcodeScalePerCol: Not enough data to read parameters, only %u bytes left.", param_max_bytes);
  if ((uint64)mAoi.getWidth() != mCount)
    ThrowRDE("OpcodeScalePerCol: Element count (%llu) does not match width of area (%d).", mCount, mAoi.getWidth());

  for (uint64 i = 0; i < mCount; i++)
    mDelta[i] = getFloat(&parameters[36+4*i]);
  *bytes_used += 4*mCount;
  mFlags = MultiThreaded;
  mDeltaX = nullptr;
}

OpcodeScalePerCol::~OpcodeScalePerCol() {
  if (mDeltaX)
    delete[] mDeltaX;
  mDeltaX = nullptr;
}


RawImage& OpcodeScalePerCol::createOutput( RawImage &in )
{
  if (mFirstPlane > in->getCpp())
    ThrowRDE("OpcodeScalePerCol: Not that many planes in actual image");

  if (mFirstPlane+mPlanes > in->getCpp())
    ThrowRDE("OpcodeScalePerCol: Not that many planes in actual image");

  if (in->getDataType() == TYPE_USHORT16) {
    if (mDeltaX)
      delete[] mDeltaX;
    int w = mAoi.getWidth();
    mDeltaX = new int[w];
    for (int i = 0; i < w; i++)
      mDeltaX[i] = (int)(1024.0f * mDelta[i]);
  }
  return in;
}

void OpcodeScalePerCol::apply( RawImage &in, RawImage &out, uint32 startY, uint32 endY )
{
  if (in->getDataType() == TYPE_USHORT16) {
    int cpp = out->getCpp();
    for (uint64 y = startY; y < endY; y += mRowPitch) {
      auto *src = (ushort16 *)out->getData(mAoi.getLeft(), y);
      // Add offset, so this is always first plane
      src+=mFirstPlane;
      for (uint64 x = 0; x < (uint64)mAoi.getWidth(); x += mColPitch) {
        for (uint64 p = 0; p < mPlanes; p++)
        {
          src[x*cpp+p] = clampBits(16, (mDeltaX[x] * src[x*cpp+p] + 512) >> 10);
        }
      }
    }
  } else {
    int cpp = out->getCpp();
    for (uint64 y = startY; y < endY; y += mRowPitch) {
      auto *src = (float *)out->getData(mAoi.getLeft(), y);
      // Add offset, so this is always first plane
      src+=mFirstPlane;
      for (uint64 x = 0; x < (uint64)mAoi.getWidth(); x += mColPitch) {
        for (uint64 p = 0; p < mPlanes; p++)
        {
          src[x*cpp+p] = mDelta[x] * src[x*cpp+p];
        }
      }
    }
  }
}


} // namespace RawSpeed
