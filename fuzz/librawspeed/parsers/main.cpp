/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017-2018 Roman Lebedev

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

#ifndef PARSER
#error PARSER must be defined
#endif

#ifndef GETDECODER
#error GETDECODER must be defined as bool
#endif

#ifndef DECODE
#error DECODE must be defined as bool
#endif

#include "io/Buffer.h"                  // for Buffer, DataBuffer
#include "io/IOException.h"             // for IOException
#include "parsers/CiffParser.h"         // IWYU pragma: keep
#include "parsers/FiffParser.h"         // IWYU pragma: keep
#include "parsers/IsoMParser.h"         // IWYU pragma: keep
#include "parsers/RawParser.h"          // IWYU pragma: keep
#include "parsers/RawParserException.h" // for RawParserException
#include "parsers/TiffParser.h"         // IWYU pragma: keep
#include <cassert>                      // for assert
#include <cstdint>                      // for uint8_t
#include <cstdio>                       // for size_t

#if GETDECODER
#include "decoders/RawDecoder.h"          // IWYU pragma: keep
#include "decoders/RawDecoderException.h" // for RawDecoderException, ThrowRDE
#if DECODE
#include "common/RawspeedException.h" // for RawspeedException
#include "metadata/CameraMetaData.h"  // for CameraMetaData
#include <memory>                     // for unique_ptr
#endif
#endif

#if GETDECODER && DECODE
static const rawspeed::CameraMetaData metadata{};
#endif

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  assert(Data);

  const rawspeed::Buffer buffer(Data, Size);

  try {
    rawspeed::PARSER parser(buffer);

#if GETDECODER
#if DECODE
    auto decoder =
#endif
        parser.getDecoder();
#endif

#if DECODE
    decoder->applyCrop = false;
    decoder->interpolateBadPixels = false;
    decoder->failOnUnknown = false;
    // decoder->checkSupport(&metadata);

    decoder->decodeRaw();
    decoder->decodeMetaData(&metadata);
#endif
  } catch (const rawspeed::RawParserException&) {
    return 0;
#if GETDECODER
  } catch (const rawspeed::RawDecoderException&) {
    return 0;
#endif
  } catch (const rawspeed::IOException&) {
    return 0;
#if DECODE
  } catch (const rawspeed::RawspeedException&) {
    return 0;
#endif
  }

  return 0;
}
