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

#include "decoders/ArwDecoder.h"
#include "common/Common.h"                          // for uint32, uchar8
#include "common/Point.h"                           // for iPoint2D
#include "common/RawspeedException.h"               // for RawspeedException
#include "decoders/RawDecoderException.h"           // for ThrowRDE
#include "decompressors/SonyArw1Decompressor.h"     // for SonyArw1Decompre...
#include "decompressors/SonyArw2Decompressor.h"     // for SonyArw2Decompre...
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "io/Buffer.h"                              // for Buffer, DataBuffer
#include "io/ByteStream.h"                          // for ByteStream
#include "io/Endianness.h"                          // for Endianness, Endi...
#include "metadata/Camera.h"                        // for Hints
#include "metadata/ColorFilterArray.h"              // for CFA_GREEN, CFA_BLUE
#include "tiff/TiffEntry.h"                         // for TiffEntry
#include "tiff/TiffIFD.h"                           // for TiffRootIFD, Tif...
#include "tiff/TiffTag.h"                           // for DNGPRIVATEDATA
#include <cassert>                                  // for assert
#include <cstring>                                  // for memcpy, size_t
#include <memory>                                   // for unique_ptr
#include <set>                                      // for set
#include <string>                                   // for operator==, string
#include <vector>                                   // for vector

using std::vector;
using std::string;

namespace rawspeed {

bool ArwDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "SONY";
}

RawImage ArwDecoder::decodeSRF(const TiffIFD* raw) {
  raw = mRootIFD->getIFDWithTag(IMAGEWIDTH);

  uint32 width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32 height = raw->getEntry(IMAGELENGTH)->getU32();

  if (width == 0 || height == 0 || width > 3360 || height > 2460)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  uint32 len = width * height * 2;

  // Constants taken from dcraw
  uint32 off = 862144;
  uint32 key_off = 200896;
  uint32 head_off = 164600;

  // Replicate the dcraw contortions to get the "decryption" key
  const uchar8* keyData = mFile->getData(key_off, 1);
  uint32 offset = (*keyData) * 4;
  keyData = mFile->getData(key_off + offset, 4);
  uint32 key = getU32BE(keyData);
  static const size_t head_size = 40;
  const uchar8* head_orig = mFile->getData(head_off, head_size);
  vector<uchar8> head(head_size);
  SonyDecrypt(reinterpret_cast<const uint32*>(head_orig),
              reinterpret_cast<uint32*>(&head[0]), 10, key);
  for (int i = 26; i > 22; i--)
    key = key << 8 | head[i - 1];

  // "Decrypt" the whole image buffer
  auto image_data = mFile->getData(off, len);
  auto image_decoded = Buffer::Create(len);
  SonyDecrypt(reinterpret_cast<const uint32*>(image_data),
              reinterpret_cast<uint32*>(image_decoded.get()), len / 4, key);

  Buffer di(move(image_decoded), len);

  // And now decode as a normal 16bit raw
  mRaw->dim = iPoint2D(width, height);
  mRaw->createData();

  UncompressedDecompressor u(di, 0, len, mRaw);
  u.decodeRawUnpacked<16, Endianness::big>(width, height);

  return mRaw;
}

RawImage ArwDecoder::decodeRawInternal() {
  const TiffIFD* raw = nullptr;
  vector<const TiffIFD*> data = mRootIFD->getIFDsWithTag(STRIPOFFSETS);

  if (data.empty()) {
    TiffEntry *model = mRootIFD->getEntryRecursive(MODEL);

    if (model && model->getString() == "DSLR-A100") {
      // We've caught the elusive A100 in the wild, a transitional format
      // between the simple sanity of the MRW custom format and the wordly
      // wonderfullness of the Tiff-based ARW format, let's shoot from the hip
      raw = mRootIFD->getIFDWithTag(SUBIFDS);
      uint32 off = raw->getEntry(SUBIFDS)->getU32();
      uint32 width = 3881;
      uint32 height = 2608;

      mRaw->dim = iPoint2D(width, height);

      ByteStream input(mFile, off);
      SonyArw1Decompressor a(mRaw);
      mRaw->createData();
      a.decompress(input);

      return mRaw;
    }

    if (hints.has("srf_format"))
      return decodeSRF(raw);

    ThrowRDE("No image data found");
  }

  raw = data[0];
  int compression = raw->getEntry(COMPRESSION)->getU32();
  if (1 == compression) {
    DecodeUncompressed(raw);
    return mRaw;
  }

  if (32767 != compression)
    ThrowRDE("Unsupported compression");

  TiffEntry *offsets = raw->getEntry(STRIPOFFSETS);
  TiffEntry *counts = raw->getEntry(STRIPBYTECOUNTS);

  if (offsets->count != 1) {
    ThrowRDE("Multiple Strips found: %u", offsets->count);
  }
  if (counts->count != offsets->count) {
    ThrowRDE(
        "Byte count number does not match strip size: count:%u, strips:%u ",
        counts->count, offsets->count);
  }
  uint32 width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32 height = raw->getEntry(IMAGELENGTH)->getU32();
  uint32 bitPerPixel = raw->getEntry(BITSPERSAMPLE)->getU32();

  switch (bitPerPixel) {
  case 8:
  case 12:
  case 14:
    break;
  default:
    ThrowRDE("Unexpected bits per pixel: %u", bitPerPixel);
  }

  // Sony E-550 marks compressed 8bpp ARW with 12 bit per pixel
  // this makes the compression detect it as a ARW v1.
  // This camera has however another MAKER entry, so we MAY be able
  // to detect it this way in the future.
  data = mRootIFD->getIFDsWithTag(MAKE);
  if (data.size() > 1) {
    for (auto &i : data) {
      string make = i->getEntry(MAKE)->getString();
      /* Check for maker "SONY" without spaces */
      if (make == "SONY")
        bitPerPixel = 8;
    }
  }

  if (width == 0 || height == 0 || height % 2 != 0 || width > 8000 ||
      height > 5320)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  bool arw1 = uint64(counts->getU32()) * 8 != width * height * bitPerPixel;
  if (arw1)
    height += 8;

  mRaw->dim = iPoint2D(width, height);

  std::vector<ushort16> curve(0x4001);
  TiffEntry *c = raw->getEntry(SONY_CURVE);
  uint32 sony_curve[] = { 0, 0, 0, 0, 0, 4095 };

  for (uint32 i = 0; i < 4; i++)
    sony_curve[i+1] = (c->getU16(i) >> 2) & 0xfff;

  for (uint32 i = 0; i < 0x4001; i++)
    curve[i] = i;

  for (uint32 i = 0; i < 5; i++)
    for (uint32 j = sony_curve[i] + 1; j <= sony_curve[i+1]; j++)
      curve[j] = curve[j-1] + (1 << i);

  RawImageCurveGuard curveHandler(&mRaw, curve, uncorrectedRawValues);

  uint32 c2 = counts->getU32();
  uint32 off = offsets->getU32();

  if (!mFile->isValid(off))
    ThrowRDE("Data offset after EOF, file probably truncated");

  if (!mFile->isValid(off, c2))
    c2 = mFile->getSize() - off;

  ByteStream input(mFile, off, c2);

  if (arw1) {
    SonyArw1Decompressor a(mRaw);
    mRaw->createData();
    a.decompress(input);
  } else
    DecodeARW2(input, width, height, bitPerPixel);

  return mRaw;
}

void ArwDecoder::DecodeUncompressed(const TiffIFD* raw) {
  uint32 width = raw->getEntry(IMAGEWIDTH)->getU32();
  uint32 height = raw->getEntry(IMAGELENGTH)->getU32();
  uint32 off = raw->getEntry(STRIPOFFSETS)->getU32();
  uint32 c2 = raw->getEntry(STRIPBYTECOUNTS)->getU32();

  mRaw->dim = iPoint2D(width, height);

  if (width == 0 || height == 0 || width > 8000 || height > 5320)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);

  if (c2 == 0)
    ThrowRDE("Strip is empty, nothing to decode!");

  const Buffer buf(mFile->getSubView(off, c2));

  mRaw->createData();

  UncompressedDecompressor u(buf, mRaw);

  if (hints.has("sr2_format"))
    u.decodeRawUnpacked<14, Endianness::big>(width, height);
  else
    u.decodeRawUnpacked<16, Endianness::little>(width, height);
}

void ArwDecoder::DecodeARW2(const ByteStream& input, uint32 w, uint32 h,
                            uint32 bpp) {

  if (bpp == 8) {
    SonyArw2Decompressor a2(mRaw, input);
    mRaw->createData();
    a2.decompress();
    return;
  } // End bpp = 8

  if (bpp == 12) {
    mRaw->createData();
    UncompressedDecompressor u(input, mRaw);
    u.decode12BitRaw<Endianness::little>(w, h);

    // Shift scales, since black and white are the same as compressed precision
    mShiftDownScale = 2;
    return;
  }
  ThrowRDE("Unsupported bit depth");
}

void ArwDecoder::ParseA100WB() {
  if (!mRootIFD->hasEntryRecursive(DNGPRIVATEDATA))
    return;

  // only contains the offset, not the length!
  TiffEntry* priv = mRootIFD->getEntryRecursive(DNGPRIVATEDATA);
  ByteStream bs = priv->getData();
  bs.setByteOrder(Endianness::little);
  const uint32 off = bs.getU32();

  bs = ByteStream(*mFile, off);

  // MRW style, see MrwDecoder

  bs.setByteOrder(Endianness::big);
  uint32 tag = bs.getU32();
  if (0x4D5249 != tag) // MRI
    ThrowRDE("Can not parse DNGPRIVATEDATA, invalid tag (0x%x).", tag);

  bs.setByteOrder(Endianness::little);
  uint32 len = bs.getU32();

  bs = bs.getSubStream(bs.getPosition(), len);

  while (bs.getRemainSize() > 0) {
    bs.setByteOrder(Endianness::big);
    tag = bs.getU32();
    bs.setByteOrder(Endianness::little);
    len = bs.getU32();
    bs.check(len);
    if (!len)
      ThrowRDE("Found entry of zero length, corrupt.");

    if (0x574247 != tag) { // WBG
      // not the tag we are interested in, skip
      bs.skipBytes(len);
      continue;
    }

    bs.skipBytes(4);

    ushort16 tmp[4];
    bs.setByteOrder(Endianness::little);
    for (auto& coeff : tmp)
      coeff = bs.getU16();

    mRaw->metadata.wbCoeffs[0] = static_cast<float>(tmp[0]);
    mRaw->metadata.wbCoeffs[1] = static_cast<float>(tmp[1]);
    mRaw->metadata.wbCoeffs[2] = static_cast<float>(tmp[3]);

    // only need this one block, no need to process any further
    break;
  }
}

void ArwDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  //Default
  int iso = 0;

  mRaw->cfa.setCFA(iPoint2D(2,2), CFA_RED, CFA_GREEN, CFA_GREEN, CFA_BLUE);

  if (mRootIFD->hasEntryRecursive(ISOSPEEDRATINGS))
    iso = mRootIFD->getEntryRecursive(ISOSPEEDRATINGS)->getU32();

  auto id = mRootIFD->getID();

  setMetaData(meta, id, "", iso);
  mRaw->whitePoint >>= mShiftDownScale;
  mRaw->blackLevel >>= mShiftDownScale;

  // Set the whitebalance
  try {
    if (id.model == "DSLR-A100") { // Handle the MRW style WB of the A100
      ParseA100WB();
    } else { // Everything else but the A100
      GetWB();
    }
  } catch (RawspeedException& e) {
    mRaw->setError(e.what());
    // We caught an exception reading WB, just ignore it
  }
}

void ArwDecoder::SonyDecrypt(const uint32* ibuf, uint32* obuf, uint32 len,
                             uint32 key) {
  if (0 == len)
    return;

  uint32 pad[128];

  // Initialize the decryption pad from the key
  for (int p=0; p < 4; p++)
    pad[p] = key = key * 48828125UL + 1UL;
  pad[3] = pad[3] << 1 | (pad[0]^pad[2]) >> 31;
  for (int p=4; p < 127; p++)
    pad[p] = (pad[p-4]^pad[p-2]) << 1 | (pad[p-3]^pad[p-1]) >> 31;
  for (int p=0; p < 127; p++)
    pad[p] = getU32BE(&pad[p]);

  int p = 127;
  // Decrypt the buffer in place using the pad
  for (; len > 0; len--) {
    pad[p & 127] = pad[(p+1) & 127] ^ pad[(p+1+64) & 127];

    uint32 pv;
    memcpy(&pv, pad + (p & 127), sizeof(uint32));

    uint32 bv;
    memcpy(&bv, ibuf, sizeof(uint32));

    bv ^= pv;

    memcpy(obuf, &bv, sizeof(uint32));

    ibuf++;
    obuf++;
    p++;
  }
}

void ArwDecoder::GetWB() {
  // Set the whitebalance for all the modern ARW formats (everything after A100)
  if (mRootIFD->hasEntryRecursive(DNGPRIVATEDATA)) {
    NORangesSet<Buffer> ifds_undecoded;

    TiffEntry *priv = mRootIFD->getEntryRecursive(DNGPRIVATEDATA);
    TiffRootIFD makerNoteIFD(nullptr, &ifds_undecoded, priv->getRootIfdData(),
                             priv->getU32());

    TiffEntry *sony_offset = makerNoteIFD.getEntryRecursive(SONY_OFFSET);
    TiffEntry *sony_length = makerNoteIFD.getEntryRecursive(SONY_LENGTH);
    TiffEntry *sony_key = makerNoteIFD.getEntryRecursive(SONY_KEY);
    if(!sony_offset || !sony_length || !sony_key || sony_key->count != 4)
      ThrowRDE("couldn't find the correct metadata for WB decoding");

    assert(sony_offset != nullptr);
    uint32 off = sony_offset->getU32();

    assert(sony_length != nullptr);
    // The Decryption is done in blocks of 4 bytes.
    uint32 len = roundDown(sony_length->getU32(), 4);

    assert(sony_key != nullptr);
    uint32 key = getU32LE(sony_key->getData(4));

    // "Decrypt" IFD
    const auto& ifd_crypt = priv->getRootIfdData();
    const auto EncryptedBuffer = ifd_crypt.getSubView(off, len);
    // We do have to prepend 'off' padding, because TIFF uses absolute offsets.
    const auto DecryptedBufferSize = off + EncryptedBuffer.getSize();
    auto DecryptedBuffer = Buffer::Create(DecryptedBufferSize);

    SonyDecrypt(reinterpret_cast<const uint32*>(EncryptedBuffer.begin()),
                reinterpret_cast<uint32*>(DecryptedBuffer.get() + off), len / 4,
                key);

    NORangesSet<Buffer> ifds_decoded;
    Buffer decIFD(std::move(DecryptedBuffer), DecryptedBufferSize);
    const Buffer Padding(decIFD.getSubView(0, off));
    // The Decrypted Root Ifd can not point to preceding padding buffer.
    ifds_decoded.emplace(Padding);

    DataBuffer dbIDD(decIFD, priv->getRootIfdData().getByteOrder());
    TiffRootIFD encryptedIFD(nullptr, &ifds_decoded, dbIDD, off);

    if (encryptedIFD.hasEntry(SONYGRBGLEVELS)){
      TiffEntry *wb = encryptedIFD.getEntry(SONYGRBGLEVELS);
      if (wb->count != 4)
        ThrowRDE("WB has %d entries instead of 4", wb->count);
      mRaw->metadata.wbCoeffs[0] = wb->getFloat(1);
      mRaw->metadata.wbCoeffs[1] = wb->getFloat(0);
      mRaw->metadata.wbCoeffs[2] = wb->getFloat(2);
    } else if (encryptedIFD.hasEntry(SONYRGGBLEVELS)){
      TiffEntry *wb = encryptedIFD.getEntry(SONYRGGBLEVELS);
      if (wb->count != 4)
        ThrowRDE("WB has %d entries instead of 4", wb->count);
      mRaw->metadata.wbCoeffs[0] = wb->getFloat(0);
      mRaw->metadata.wbCoeffs[1] = wb->getFloat(1);
      mRaw->metadata.wbCoeffs[2] = wb->getFloat(3);
    }
  }
}

} // namespace rawspeed
