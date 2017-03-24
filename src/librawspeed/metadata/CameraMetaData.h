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

#include "common/Common.h" // for uint32
#include <map>             // for map
#include <string>          // for string
#include <tuple>           // for tuple

namespace RawSpeed {

class Camera;

struct CameraId {
  std::string make;
  std::string model;
  std::string mode;

  bool operator<(const CameraId& rhs) const {
    return std::tie(make, model, mode) <
           std::tie(rhs.make, rhs.model, rhs.mode);
  }
};

class CameraMetaData final {
public:
  CameraMetaData() = default;
  explicit CameraMetaData(const char* docname);
  ~CameraMetaData();
  std::map<CameraId, Camera*> cameras;
  std::map<uint32,Camera*> chdkCameras;

  // searches for camera with given make + model + mode
  const Camera* getCamera(const std::string& make, const std::string& model,
                          const std::string& mode) const;

  // searches for camera with given make + model, with ANY mode
  const Camera* getCamera(const std::string& make,
                          const std::string& model) const;

  bool hasCamera(const std::string& make, const std::string& model,
                 const std::string& mode) const;
  const Camera* __attribute__((pure)) getChdkCamera(uint32 filesize) const;
  bool __attribute__((pure)) hasChdkCamera(uint32 filesize) const;
  void disableMake(const std::string &make);
  void disableCamera(const std::string &make, const std::string &model);

protected:
  bool addCamera(Camera* cam);
};

} // namespace RawSpeed
