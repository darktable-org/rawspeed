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

#include "ThreadSafetyAnalysis.h" // for REQUIRES, GUARDED_BY
#include "common/Mutex.h"         // for Mutex
#include <string>                 // for string
#include <vector>                 // for vector

namespace rawspeed {

class ErrorLog {
  Mutex mutex;
  std::vector<std::string> errors GUARDED_BY(mutex);

public:
  void setError(const std::string& err) REQUIRES(!mutex);
  bool isTooManyErrors(unsigned many, std::string* firstErr = nullptr)
      REQUIRES(!mutex);
  std::vector<std::string>&& getErrors() REQUIRES(!mutex);
};

} // namespace rawspeed
