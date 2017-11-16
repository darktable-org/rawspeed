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

#ifndef DECODER
#error DECODER must be defined
#endif

#include "common/RawspeedException.h" // for RawspeedException
#include "decoders/ArwDecoder.h"      // IWYU pragma: keep
#include "decoders/Cr2Decoder.h"      // IWYU pragma: keep
#include "decoders/DcrDecoder.h"      // IWYU pragma: keep
#include "decoders/DcsDecoder.h"      // IWYU pragma: keep
#include "decoders/DngDecoder.h"      // IWYU pragma: keep
#include "decoders/ErfDecoder.h"      // IWYU pragma: keep
#include "decoders/IiqDecoder.h"      // IWYU pragma: keep
#include "decoders/KdcDecoder.h"      // IWYU pragma: keep
#include "decoders/MefDecoder.h"      // IWYU pragma: keep
#include "decoders/MosDecoder.h"      // IWYU pragma: keep
#include "decoders/NefDecoder.h"      // IWYU pragma: keep
#include "decoders/OrfDecoder.h"      // IWYU pragma: keep
#include "decoders/PefDecoder.h"      // IWYU pragma: keep
#include "decoders/RafDecoder.h"      // IWYU pragma: keep
#include "decoders/Rw2Decoder.h"      // IWYU pragma: keep
#include "decoders/SrwDecoder.h"      // IWYU pragma: keep
#include "decoders/ThreefrDecoder.h"  // IWYU pragma: keep
#include "io/Buffer.h"                // for Buffer, DataBuffer
#include "metadata/CameraMetaData.h"  // for CameraMetaData
#include "parsers/TiffParser.h"       // for TiffParser
#include <algorithm>                  // for move
#include <cassert>                    // for assert
#include <cstdint>                    // for uint8_t
#include <cstdio>                     // for size_t
#include <memory>                     // for unique_ptr

static const rawspeed::CameraMetaData metadata{};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

using rawspeed::DECODER;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  try {
    const rawspeed::Buffer buffer(Data, Size);
    auto ifd = rawspeed::TiffParser::parse(buffer);

    // ATM do not care if this is the appropriate decoder.
    // only check that the check does not crash.
    (void)DECODER::isAppropriateDecoder(ifd.get(), &buffer);

    auto decoder = std::make_unique<DECODER>(std::move(ifd), &buffer);

    decoder->applyCrop = false;
    decoder->interpolateBadPixels = false;
    decoder->failOnUnknown = false;
    // decoder->checkSupport(&metadata);

    decoder->decodeRaw();
    decoder->decodeMetaData(&metadata);

  } catch (rawspeed::RawspeedException&) {
    return 0;
  }

  return 0;
}
