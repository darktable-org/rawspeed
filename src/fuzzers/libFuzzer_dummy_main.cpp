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

#include "io/Buffer.h"      // for Buffer
#include "io/FileReader.h"  // for FileReader
#include "io/IOException.h" // for IOException
#include <cstdint>          // for uint8_t
#include <cstdlib>          // for EXIT_SUCCESS, size_t
#include <iostream>         // for operator<<, cout, ostream
#include <memory>           // for unique_ptr

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);

static int usage() {
  std::cout << "This is just a placeholder.\nFor fuzzers to actually function, "
               "you need to build rawspeed with clang compiler, with FUZZ "
               "build type.\n";

  return EXIT_SUCCESS;
}

static void process(const char* filename) {
  try {
    rawspeed::FileReader reader(filename);
    auto map(reader.readFile());
    LLVMFuzzerTestOneInput(map->getData(0, map->getSize()), map->getSize());
  } catch (rawspeed::IOException&) {
    // failed to read the file for some reason.
    // just ignore it.
    return;
  }
}

int main(int argc, char** argv) {
  if (1 == argc)
    return usage();

#ifdef _OPENMP
#pragma omp parallel for default(shared) schedule(static)
#endif
  for (int i = 1; i < argc; ++i)
    process(argv[i]);

  return EXIT_SUCCESS;
}
