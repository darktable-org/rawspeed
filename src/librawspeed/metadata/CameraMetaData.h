/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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

#include "rawspeedconfig.h"  // for HAVE_PUGIXML
#include "metadata/Camera.h" // IWYU pragma: keep
#include <cstdint>           // for uint32_t
#include <map>               // for map
#include <memory>            // for unique_ptr
#include <string>            // for string
#include <tuple>             // for tie, operator<, tuple

namespace rawspeed {

struct CameraId {
  std::string make;
  std::string model;
  std::string mode;

  bool operator<(const CameraId& rhs) const {
    return std::tie(make, model, mode) <
           std::tie(rhs.make, rhs.model, rhs.mode);
  }
};

// NOTE: *NOT* `final`, could be derived from by downstream.
class CameraMetaData {
public:
  CameraMetaData() = default;

#ifdef HAVE_PUGIXML
  explicit CameraMetaData(const char* docname);
#endif

  std::map<CameraId, std::unique_ptr<Camera>> cameras;
  std::map<uint32_t, Camera*> chdkCameras;

  // searches for camera with given make + model + mode
  [[nodiscard]] const Camera* getCamera(const std::string& make,
                                        const std::string& model,
                                        const std::string& mode) const;

  // searches for camera with given make + model, with ANY mode
  [[nodiscard]] const Camera* getCamera(const std::string& make,
                                        const std::string& model) const;

  [[nodiscard]] bool hasCamera(const std::string& make,
                               const std::string& model,
                               const std::string& mode) const;
  [[nodiscard]] const Camera* __attribute__((pure))
  getChdkCamera(uint32_t filesize) const;
  [[nodiscard]] bool __attribute__((pure))
  hasChdkCamera(uint32_t filesize) const;
  void disableMake(std::string_view make) const;
  void disableCamera(std::string_view make, std::string_view model) const;

private:
  const Camera* addCamera(std::unique_ptr<Camera> cam);
};

} // namespace rawspeed
