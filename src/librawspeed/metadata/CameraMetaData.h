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

namespace RawSpeed {

class Camera;

class CameraMetaData
{
public:
  CameraMetaData();
  CameraMetaData(const char *docname);
  virtual ~CameraMetaData(void);
  std::map<std::string,Camera*> cameras;
  std::map<uint32,Camera*> chdkCameras;

  // searches for camera with given make + model + mode
  Camera* getCamera(std::string make, std::string model, std::string mode);

  // searches for camera with given make + model, with ANY mode
  Camera* getCamera(std::string make, std::string model);

  bool hasCamera(std::string make, std::string model, std::string mode);
  Camera* getChdkCamera(uint32 filesize);
  bool hasChdkCamera(uint32 filesize);
  void disableMake(const std::string &make);
  void disableCamera(const std::string &make, const std::string &model);

protected:
  bool addCamera(Camera* cam);
};

} // namespace RawSpeed
