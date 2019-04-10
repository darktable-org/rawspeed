/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

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

#include "rawspeedconfig.h"          // for HAVE_PUGIXML
#include "metadata/CameraMetaData.h" // for CameraMetaData
#include <benchmark/benchmark.h>     // for Benchmark, State, BENCHMARK_MAIN
#include <pugixml.hpp>               // for xml_document

#ifndef HAVE_PUGIXML
#error This benchmark requires to be built with pugixml being present.
#endif

static constexpr const char* const CAMERASXML =
    RAWSPEED_SOURCE_DIR "/data/cameras.xml";

static void BM_pugixml_load_cameras_xml(benchmark::State& state) {
  for (auto _ : state) {
    pugi::xml_document doc;

#if defined(__unix__) || defined(__APPLE__)
    pugi::xml_parse_result result = doc.load_file(CAMERASXML);
#else
    pugi::xml_parse_result result =
        doc.load_file(pugi::as_wide(CAMERASXML).c_str());
#endif

    benchmark::DoNotOptimize(doc);
    benchmark::DoNotOptimize(result);
  }
}
BENCHMARK(BM_pugixml_load_cameras_xml)->Unit(benchmark::kMicrosecond);

static void BM_CameraMetaData(benchmark::State& state) {
  for (auto _ : state) {
    rawspeed::CameraMetaData metadata(CAMERASXML);
    benchmark::DoNotOptimize(metadata);
  }
}
BENCHMARK(BM_CameraMetaData)->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
